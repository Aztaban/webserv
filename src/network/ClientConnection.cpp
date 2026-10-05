#include "network/ClientConnection.hpp"
#include "utils/Logger.hpp"
#include "core/RequestHandler.hpp"
#include "http/HttpRequestParser.hpp"
#include "http/HttpResponseBuilder.hpp"
#include "core/ErrorResponseFactory.hpp"
#include "routing/Router.hpp"

#include <unistd.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <cstdio>
#include <sstream>

ClientConnection::ClientConnection(int fd, const RequestContext& context)
	: _fd(fd),
	  _state(CLIENT_READING_HEADERS),
	  _readBuffer(),
	  _writeBuffer(),
	  _writeOffset(0),
	  _writeBody(),
	  _writeBodyOffset(0),
	  _lastActivity(std::time(NULL)),
	  _context(context),
	  _pendingCgiRequest(),
	  _spoolingCgiBody(false),
	  _cgiBodyFileFd(-1),
	  _cgiBodyFilePath(),
	  _cgiBodyExpected(0),
	  _cgiBodyReceived(0),
	  _spoolingChunkedCgi(false),
	  _cgiBodyLimit(0),
	  _cgiChunkBuffer(),
	  _cgiChunkRemaining(0),
	  _cgiChunkState(0)
{
}

static const std::size_t DEFAULT_CLIENT_MAX_BODY_SIZE = 1000000;
static const int CGI_CHUNK_SIZE = 0;
static const int CGI_CHUNK_DATA = 1;
static const int CGI_CHUNK_CRLF = 2;
static const int CGI_CHUNK_TRAILERS = 3;

static bool parseCgiChunkSize(const std::string& rawLine, std::size_t& size)
{
	std::string line = rawLine;
	std::size_t semi = line.find(';');
	if (semi != std::string::npos)
		line.erase(semi);
	std::size_t first = line.find_first_not_of(" \t");
	std::size_t last = line.find_last_not_of(" \t");
	if (first == std::string::npos)
		return false;
	line = line.substr(first, last - first + 1);
	size = 0;
	for (std::size_t i = 0; i < line.size(); ++i)
	{
		unsigned char c = static_cast<unsigned char>(line[i]);
		std::size_t value;
		if (c >= '0' && c <= '9') value = c - '0';
		else if (c >= 'a' && c <= 'f') value = c - 'a' + 10;
		else if (c >= 'A' && c <= 'F') value = c - 'A' + 10;
		else return false;
		if (size > (static_cast<std::size_t>(-1) - value) / 16)
			return false;
		size = size * 16 + value;
	}
	return true;
}

ClientConnection::~ClientConnection()
{
	if (_cgiBodyFileFd != -1)
		close(_cgiBodyFileFd);
	if (!_cgiBodyFilePath.empty())
		std::remove(_cgiBodyFilePath.c_str());
	if (_fd != -1)
		close(_fd);
}

bool ClientConnection::_writeCgiBodyFile(const char* data, std::size_t size)
{
	std::size_t offset = 0;
	while (offset < size)
	{
		ssize_t written = write(_cgiBodyFileFd, data + offset, size - offset);
		if (written <= 0)
			return false;
		offset += static_cast<std::size_t>(written);
	}
	return true;
}

void ClientConnection::_finishCgiBodySpool()
{
	if (_cgiBodyFileFd != -1)
	{
		close(_cgiBodyFileFd);
		_cgiBodyFileFd = -1;
	}
	_spoolingCgiBody = false;
	_spoolingChunkedCgi = false;
	_pendingCgiRequest.bodyFilePath.swap(_cgiBodyFilePath);
	std::ostringstream length;
	length << _cgiBodyReceived;
	for (std::size_t i = 0; i < _pendingCgiRequest.env.size(); ++i)
	{
		if (_pendingCgiRequest.env[i].compare(0, 15, "CONTENT_LENGTH=") == 0)
		{
			_pendingCgiRequest.env[i] = "CONTENT_LENGTH=" + length.str();
			break;
		}
	}
	_state = CLIENT_WAITING_CGI;
}

bool ClientConnection::_consumeChunkedCgiData(const char* data, std::size_t size)
{
	_cgiChunkBuffer.append(data, size);
	while (true)
	{
		if (_cgiChunkState == CGI_CHUNK_SIZE)
		{
			std::size_t lineEnd = _cgiChunkBuffer.find("\r\n");
			if (lineEnd == std::string::npos)
			{
				if (_cgiChunkBuffer.size() > 8192)
					prepareErrorResponse(400);
				return _cgiChunkBuffer.size() <= 8192;
			}
			std::size_t chunkSize;
			if (!parseCgiChunkSize(_cgiChunkBuffer.substr(0, lineEnd), chunkSize))
			{
				prepareErrorResponse(400);
				return false;
			}
			if (chunkSize > _cgiBodyLimit - _cgiBodyReceived)
			{
				prepareErrorResponse(413);
				return false;
			}
			_cgiChunkBuffer.erase(0, lineEnd + 2);
			if (chunkSize == 0)
				_cgiChunkState = CGI_CHUNK_TRAILERS;
			else
			{
				_cgiChunkRemaining = chunkSize;
				_cgiChunkState = CGI_CHUNK_DATA;
			}
		}
		else if (_cgiChunkState == CGI_CHUNK_DATA)
		{
			if (_cgiChunkBuffer.empty())
				return true;
			std::size_t amount = _cgiChunkBuffer.size();
			if (amount > _cgiChunkRemaining)
				amount = _cgiChunkRemaining;
			if (!_writeCgiBodyFile(_cgiChunkBuffer.data(), amount))
			{
				prepareErrorResponse(500);
				return false;
			}
			_cgiChunkBuffer.erase(0, amount);
			_cgiChunkRemaining -= amount;
			_cgiBodyReceived += amount;
			if (_cgiChunkRemaining == 0)
				_cgiChunkState = CGI_CHUNK_CRLF;
			else
				return true;
		}
		else if (_cgiChunkState == CGI_CHUNK_CRLF)
		{
			if (_cgiChunkBuffer.size() < 2)
				return true;
			if (_cgiChunkBuffer.compare(0, 2, "\r\n") != 0)
			{
				prepareErrorResponse(400);
				return false;
			}
			_cgiChunkBuffer.erase(0, 2);
			_cgiChunkState = CGI_CHUNK_SIZE;
		}
		else
		{
			if (_cgiChunkBuffer.size() >= 2
				&& _cgiChunkBuffer.compare(0, 2, "\r\n") == 0)
			{
				_cgiChunkBuffer.erase(0, 2);
				_finishCgiBodySpool();
				return true;
			}
			std::size_t trailerEnd = _cgiChunkBuffer.find("\r\n\r\n");
			if (trailerEnd != std::string::npos)
			{
				_cgiChunkBuffer.erase(0, trailerEnd + 4);
				_finishCgiBodySpool();
				return true;
			}
			if (_cgiChunkBuffer.size() > 8192)
			{
				prepareErrorResponse(400);
				return false;
			}
			return true;
		}
	}
}

bool ClientConnection::_startCgiBodySpool(const HttpRequest& request,
	std::size_t headerEnd, std::size_t bodyLimit, bool chunked)
{
	Router router;
	const ServerConfig* selectedServer = router.matchServer(*_context.serverCandidates, request);
	if (!selectedServer)
		return false;
	const LocationConfig* location = router.matchLocation(request, *selectedServer);
	if (!location)
		return false;
	std::string requestRoot = location->root.empty() ? selectedServer->getRoot() : location->root;
	std::string scriptPath;
	std::string interpreter;
	if (!RequestHandler::_isCgiRequest(request, *location, requestRoot, scriptPath, interpreter))
		return false;

	RequestContext effectiveContext = _context;
	effectiveContext.selectedServer = selectedServer;
	RequestResult routed = RequestHandler::handle(request, effectiveContext);
	if (routed.type != RequestResult::CGI_REQUIRED)
		return false;

	static unsigned long spoolCounter = 0;
	std::ostringstream path;
	path << "/tmp/webserv_cgi_body_" << _fd << "_" << spoolCounter++;
	_cgiBodyFilePath = path.str();
	_cgiBodyFileFd = open(_cgiBodyFilePath.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
	if (_cgiBodyFileFd == -1)
	{
		prepareErrorResponse(500);
		return true;
	}

	_pendingCgiRequest.swap(routed.cgiRequest);
	_context.selectedServer = selectedServer;
	_spoolingCgiBody = true;
	_spoolingChunkedCgi = chunked;
	_cgiBodyLimit = bodyLimit;
	_cgiBodyExpected = chunked ? 0 : request.contentLength;
	_cgiBodyReceived = 0;
	_cgiChunkRemaining = 0;
	_cgiChunkState = CGI_CHUNK_SIZE;
	_cgiChunkBuffer.clear();

	std::size_t bodyStart = headerEnd + 4;
	std::size_t bufferedBody = _readBuffer.size() - bodyStart;
	bool ok = true;
	if (bufferedBody > 0)
	{
		if (chunked)
			ok = _consumeChunkedCgiData(_readBuffer.data() + bodyStart, bufferedBody);
		else
		{
			std::size_t amount = bufferedBody;
			if (amount > _cgiBodyExpected)
				amount = _cgiBodyExpected;
			ok = _writeCgiBodyFile(_readBuffer.data() + bodyStart, amount);
			_cgiBodyReceived = amount;
			if (ok && _cgiBodyReceived >= _cgiBodyExpected)
				_finishCgiBodySpool();
		}
	}
	_readBuffer.erase(bodyStart);
	if (!ok)
		return true;
	if (_state != CLIENT_WAITING_CGI)
		_state = CLIENT_READING_BODY;
	return true;
}

int ClientConnection::getFd() const
{
	return (_fd);
}

CgiRequest&	ClientConnection::getCgiRequest()
{
	return (_pendingCgiRequest);
}

ClientState ClientConnection::getState() const
{
	return (_state);
}

bool ClientConnection::wantsRead() const
{
	return (_state == CLIENT_READING_HEADERS || _state == CLIENT_READING_BODY);
}

bool ClientConnection::wantsWrite() const
{
	return (_state == CLIENT_WRITING);
}

bool ClientConnection::isDone() const
{
	return (_state == CLIENT_DONE);
}

void ClientConnection::readFromSocket()
{
	// A larger single non-blocking recv keeps large request bodies practical
	// without changing the one-read-per-poll-event rule.  The subject tester
	// sends a 100,000,000-byte CGI body; 4 KiB reads require ~24k poll cycles.
	char	buffer[65536];
	ssize_t	bytes = recv(_fd, buffer, sizeof(buffer), 0);

	if (bytes <= 0) // no errno check after recv()
	{
		Logger::debug("Client fd " + toString(_fd) + " disconnected during read");
		_state = CLIENT_DONE;
		return;
	}
	touch();

	Logger::debug("Client fd " + toString(_fd) + " read " + toString(bytes) + " byte(s)");

	if (_spoolingCgiBody)
	{
		if (_spoolingChunkedCgi)
		{
			_consumeChunkedCgiData(buffer, static_cast<std::size_t>(bytes));
			return;
		}
		std::size_t remaining = _cgiBodyExpected - _cgiBodyReceived;
		std::size_t toWrite = static_cast<std::size_t>(bytes);
		if (toWrite > remaining)
			toWrite = remaining;
		if (toWrite > 0 && !_writeCgiBodyFile(buffer, toWrite))
		{
			prepareErrorResponse(500);
			return;
		}
		_cgiBodyReceived += toWrite;
		if (_cgiBodyReceived >= _cgiBodyExpected)
		{
			_finishCgiBodySpool();
		}
		return;
	}

	_readBuffer.append(buffer, bytes);

	if (_readBuffer.find("\r\n\r\n") == std::string::npos
		&& _readBuffer.size() > MAX_HEADER_SIZE)
	{
		Logger::error("Client fd " + toString(_fd) + " header too large");
		prepareErrorResponse(431);
		_state = CLIENT_WRITING;
		return;
	}
	_processReadBuffer();
}

void ClientConnection::writeToSocket()
{
	ssize_t	bytes;

	if (_writeOffset < _writeBuffer.size())
	{
		bytes = send(_fd, _writeBuffer.c_str() + _writeOffset,
			_writeBuffer.size() - _writeOffset, 0);
		if (bytes <= 0)
		{
			Logger::debug("Client fd " + toString(_fd) + " disconnected during write");
			_state = CLIENT_DONE;
			return;
		}
		touch();
		_writeOffset += static_cast<std::size_t>(bytes);
		Logger::debug("Client fd " + toString(_fd) + " sent " + toString(bytes) + " byte(s)");
		return;
	}
	if (_writeBodyOffset >= _writeBody.size())
	{
		_state = CLIENT_DONE;
		return;
	}
	bytes = send(_fd, _writeBody.c_str() + _writeBodyOffset,
		_writeBody.size() - _writeBodyOffset, 0);
	if (bytes <= 0)
	{
		Logger::debug("Client fd " + toString(_fd) + " disconnected during write");
		_state = CLIENT_DONE;
		return;
	}
	touch();

	Logger::debug("Client fd " + toString(_fd) + " sent " + toString(bytes) + " byte(s)");

	_writeBodyOffset += static_cast<std::size_t>(bytes);
	if (_writeBodyOffset >= _writeBody.size())
		_state = CLIENT_DONE; // Later: reset to READING_HEADERS here if keep-alive is supported
}

void	ClientConnection::prepareResponse(const HttpResponse& response)
{
	std::string().swap(_writeBody);
	_writeBodyOffset = 0;
	// Apply configured pages at the shared boundary for static, parser and CGI errors.
	if (response.statusCode >= 400 && response.statusCode <= 599)
	{
		HttpResponse effectiveResponse = response;
		ErrorResponseFactory::applyCustomPage(effectiveResponse, _context);
		_writeBuffer = HttpResponseBuilder::build(effectiveResponse);
	}
	else
		_writeBuffer = HttpResponseBuilder::build(response);
	_writeOffset = 0;
	// HEAD responses carry the normal headers, including representation length,
	// but never a message body, including for redirects and error responses.
	if (_readBuffer.compare(0, 5, "HEAD ") == 0)
	{
		std::size_t headerEnd = _writeBuffer.find("\r\n\r\n");
		if (headerEnd != std::string::npos)
			_writeBuffer.erase(headerEnd + 4);
	}
	_state = CLIENT_WRITING;
	touch();

	Logger::debug("Client fd " + toString(_fd) + " switched to CLIENT_WRITING");
}

void	ClientConnection::prepareOwnedResponse(HttpResponse& response)
{
	if (response.statusCode >= 400 && response.statusCode <= 599)
	{
		ErrorResponseFactory::applyCustomPage(response, _context);
		prepareResponse(response);
		return;
	}

	_writeBuffer = HttpResponseBuilder::buildHeaders(response, response.body.size());
	_writeOffset = 0;
	_writeBodyOffset = 0;
	_writeBody.swap(response.body);
	if (_readBuffer.compare(0, 5, "HEAD ") == 0)
		std::string().swap(_writeBody);
	_state = CLIENT_WRITING;
	touch();
	Logger::debug("Client fd " + toString(_fd) + " switched to CLIENT_WRITING");
}

void ClientConnection::touch()
{
	_lastActivity = std::time(NULL);
}

bool ClientConnection::isTimedOut(std::time_t now, int timeoutSeconds) const
{
	return (now - _lastActivity >= timeoutSeconds);
}

void ClientConnection::_processReadBuffer()
{
	HttpRequestParser				parser;
	HttpRequestParser::ParseStatus	status;
	std::size_t					bodyLimit;
	std::size_t					headerEnd;

	bodyLimit = _getClientMaxBodySize();
	headerEnd = _readBuffer.find("\r\n\r\n");

	// For Content-Length requests, validate the headers and declared size as soon
	// as the header block is complete, then wait until the body is fully buffered
	// before doing the full parse.  This avoids reparsing/copying an ever-growing
	// body on every 4 KiB socket read (important for the subject tester's 100 MB
	// CGI request).
	if (headerEnd != std::string::npos)
	{
		HttpRequestParser headerParser;
		HttpRequestParser::ParseStatus headerStatus = headerParser.parse(
			_readBuffer.substr(0, headerEnd + 4), bodyLimit);

		if (headerStatus == HttpRequestParser::PARSE_ERROR)
		{
			prepareErrorResponse(headerParser.getErrorStatusCode());
			return;
		}
		if (headerParser.headersComplete())
		{
			const HttpRequest& headerRequest = headerParser.getRequest();
			if (headerRequest.isChunked)
			{
				if (_startCgiBodySpool(headerRequest, headerEnd, bodyLimit, true))
					return;
				const std::size_t chunkedReparseThreshold = 65536;
				std::size_t bufferedBody = _readBuffer.size() - (headerEnd + 4);
				bool couldBeComplete = (bufferedBody >= 4
					&& _readBuffer.compare(_readBuffer.size() - 4, 4, "\r\n\r\n") == 0);
				if (bufferedBody > chunkedReparseThreshold && !couldBeComplete)
				{
					_state = CLIENT_READING_BODY;
					return;
				}
			}
			else if (headerRequest.hasBody)
			{
				std::size_t bufferedBody = _readBuffer.size() - (headerEnd + 4);
				const std::size_t cgiSpoolThreshold = 1024 * 1024;
				if (headerRequest.contentLength >= cgiSpoolThreshold
					&& bufferedBody < headerRequest.contentLength
					&& _startCgiBodySpool(headerRequest, headerEnd, bodyLimit, false))
					return;
				if (bufferedBody < headerRequest.contentLength)
				{
					_state = CLIENT_READING_BODY;
					return;
				}
			}
		}
	}

	status = parser.parse(_readBuffer, bodyLimit);

	if (status == HttpRequestParser::PARSE_ERROR)
	{
		Logger::error("Client fd " + toString(_fd) + " malformed request");
		prepareErrorResponse(parser.getErrorStatusCode());
		return;
	}

	if (parser.headersComplete())
	{
		const HttpRequest& request = parser.getRequest();

		if (request.hasBody && request.contentLength > bodyLimit)
		{
			Logger::error("Client fd " + toString(_fd) + " request body too large");
			prepareErrorResponse(413);
			return;
		}

		if (status == HttpRequestParser::PARSE_INCOMPLETE)
		{
			_state = CLIENT_READING_BODY;
			return;
		}
	}

	if (status == HttpRequestParser::PARSE_INCOMPLETE)
		return;

	_state = CLIENT_PROCESSING;

	RequestContext effectiveContext = _context;
	Router router;
	const ServerConfig* selectedServer = router.matchServer(*_context.serverCandidates, parser.getRequest());
	if (selectedServer == NULL)
	{
		prepareErrorResponse(400);
		return;
	}
	effectiveContext.selectedServer = selectedServer;
	_context.selectedServer = selectedServer;

	RequestResult response = RequestHandler::handle(parser.getRequest(), effectiveContext);
	if (response.type == RequestResult::RESPONSE_READY)
		prepareResponse(response.response);
	else if (response.type == RequestResult::CGI_REQUIRED)
	{
		// Routing only builds CGI metadata. Transfer the parsed request body and
		// then the CGI request itself without duplicating a potentially 100 MB body.
		parser.swapBody(response.cgiRequest.body);
		_pendingCgiRequest.swap(response.cgiRequest);
		std::string().swap(_readBuffer);
		_state = CLIENT_WAITING_CGI;
	}
}

std::size_t ClientConnection::_getClientMaxBodySize() const
{
	if (_context.serverCandidates && !_context.serverCandidates->empty())
	{
		HttpRequestParser parser;
		std::size_t headerEnd = _readBuffer.find("\r\n\r\n");
		HttpRequestParser::ParseStatus status = parser.parse(
			headerEnd == std::string::npos ? _readBuffer : _readBuffer.substr(0, headerEnd + 4));
		if (parser.headersComplete())
		{
			Router router;
			const ServerConfig* selectedServer = _context.selectedServer;
			if (!selectedServer)
				selectedServer = router.matchServer(*_context.serverCandidates, parser.getRequest());
			if (selectedServer)
			{
				const LocationConfig* location = router.matchLocation(parser.getRequest(), *selectedServer);
				if (location && location->hasClientMaxBodySize)
					return location->clientMaxBodySize;
				return selectedServer->getClientMaxBodySize();
			}
		}
		if (status == HttpRequestParser::PARSE_ERROR)
			return ((*_context.serverCandidates)[0].getClientMaxBodySize());
		if (_context.selectedServer)
			return _context.selectedServer->getClientMaxBodySize();
		return ((*_context.serverCandidates)[0].getClientMaxBodySize());
	}

	return (DEFAULT_CLIENT_MAX_BODY_SIZE);
}

void ClientConnection::prepareErrorResponse(int code)
{
	// Errors such as 413 can occur before normal request dispatch selects a host.
	if (!_context.selectedServer && _context.serverCandidates)
	{
		std::size_t headerEnd = _readBuffer.find("\r\n\r\n");
		if (headerEnd != std::string::npos)
		{
			HttpRequestParser parser;
			parser.parse(_readBuffer.substr(0, headerEnd + 4));
			if (parser.headersComplete())
			{
				Router router;
				_context.selectedServer = router.matchServer(*_context.serverCandidates, parser.getRequest());
			}
		}
	}
	HttpResponse	response;

	response = ErrorResponseFactory::create(code, _context);

	prepareResponse(response);
}
