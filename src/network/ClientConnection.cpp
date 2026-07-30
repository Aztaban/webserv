#include "network/ClientConnection.hpp"
#include "utils/Logger.hpp"
#include "core/RequestHandler.hpp"
#include "http/HttpHeaderUtils.hpp"
#include "http/HttpRequestParser.hpp"
#include "http/HttpResponseBuilder.hpp"
#include "core/ErrorResponseFactory.hpp"
#include "routing/Router.hpp"

#include <unistd.h>
#include <sys/socket.h>

static const std::size_t DEFAULT_CLIENT_MAX_BODY_SIZE = 1000000;

ClientConnection::ClientConnection(int fd, const RequestContext& context)
	: _fd(fd),
	  _state(CLIENT_READING_HEADERS),
	  _readBuffer(),
	  _writeBuffer(),
	  _lastActivity(std::time(NULL)),
	  _context(context),
	  _selectedServer(NULL),
	  _discardBodyBytesRemaining(0)
{
}

ClientConnection::~ClientConnection()
{
	if (_fd != -1)
		close(_fd);
}

int ClientConnection::getFd() const
{
	return (_fd);
}

ClientState ClientConnection::getState() const
{
	return (_state);
}

bool ClientConnection::wantsRead() const
{
	return (_state == CLIENT_READING_HEADERS || _state == CLIENT_READING_BODY || (_state == CLIENT_REJECTING_BODY && _discardBodyBytesRemaining > 0));
}

bool ClientConnection::wantsWrite() const
{
	return (_state == CLIENT_WRITING || (_state == CLIENT_REJECTING_BODY && !_writeBuffer.empty()));
}

bool ClientConnection::isDone() const
{
	return (_state == CLIENT_DONE);
}

void ClientConnection::readFromSocket()
{
	char	buffer[4096];
	ssize_t	bytes = recv(_fd, buffer, sizeof(buffer), 0);

	if (bytes <= 0) // no errno check after recv()
	{
		Logger::debug("Client fd " + toString(_fd) + " disconnected during read");
		if (_state == CLIENT_REJECTING_BODY && !_writeBuffer.empty())
			_state = CLIENT_WRITING;
		else
			_state = CLIENT_DONE;
		return;
	}
	touch();

	Logger::debug("Client fd " + toString(_fd) + " read " + toString(bytes) + " byte(s)");

	if (_state == CLIENT_REJECTING_BODY)
	{
		const std::size_t received = static_cast<std::size_t>(bytes);
		if (received >= _discardBodyBytesRemaining)
			_discardBodyBytesRemaining = 0;
		else
			_discardBodyBytesRemaining -= received;

		if (_discardBodyBytesRemaining == 0)
		{
			if (_writeBuffer.empty())
				_state = CLIENT_DONE;
			else
				_state = CLIENT_WRITING;
		}
		return;
	}

	_readBuffer.append(buffer, bytes);

	if (_readBuffer.find("\r\n\r\n") == std::string::npos
		&& _readBuffer.size() > MAX_HEADER_SIZE)
	{
		Logger::error("Client fd " + toString(_fd) + " header too large");
		_prepareErrorResponse(431);
		_state = CLIENT_WRITING;
		return;
	}
	_processReadBuffer();
}

void ClientConnection::writeToSocket()
{
	ssize_t	bytes;
	const bool rejectingBody = (_state == CLIENT_REJECTING_BODY);

	if (_writeBuffer.empty())
	{
		if (!rejectingBody || _discardBodyBytesRemaining == 0)
			_state = CLIENT_DONE;
		return;
	}
	bytes = send(_fd, _writeBuffer.c_str(), _writeBuffer.size(), 0);
	if (bytes <= 0)
	{
		Logger::debug("Client fd " + toString(_fd) + " disconnected during write");
		_state = CLIENT_DONE;
		return;
	}
	touch();

	Logger::debug("Client fd " + toString(_fd) + " sent " + toString(bytes) + " byte(s)");

	_writeBuffer.erase(0, bytes);
	if (_writeBuffer.empty() && (!rejectingBody || _discardBodyBytesRemaining == 0))
		_state = CLIENT_DONE;
}

void	ClientConnection::prepareResponse(const HttpResponse& response)
{
	_writeBuffer = HttpResponseBuilder::build(response);
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

const ServerConfig* ClientConnection::_selectServerForRequest(
	const HttpRequest& request,
	int& errorStatusCode)
{
	std::string hostHeader;

	errorStatusCode = 0;
	if (!HttpHeaderUtils::getValue(request.headers, "Host", hostHeader))
	{
		errorStatusCode = 400;
		if (_context.serverCandidates && !_context.serverCandidates->empty())
			return &(*_context.serverCandidates)[0];
		return NULL;
	}
	return Router::selectServerFromHost(hostHeader, _context, errorStatusCode);
}

void ClientConnection::_processReadBuffer()
{
	HttpRequestParser				parser;
	HttpRequestParser::ParseStatus	status;
	int hostError = 0;

	status = parser.parse(_readBuffer);

	if (parser.headersComplete())
	{
		const HttpRequest& request = parser.getRequest();
		_selectedServer = _selectServerForRequest(request, hostError);
		if (hostError != 0)
		{
			_prepareErrorResponse(hostError);
			return;
		}

		const std::size_t limit = _getClientMaxBodySize();
		if (request.hasBody && request.contentLength > limit)
		{
			const std::size_t headerEnd = _readBuffer.find("\r\n\r\n");
			std::size_t receivedBodySize = 0;
			if (headerEnd != std::string::npos
				&& _readBuffer.size() > headerEnd + 4)
				receivedBodySize = _readBuffer.size() - (headerEnd + 4);

			Logger::error("Client fd " + toString(_fd)
				+ " request body too large");
			_rejectRequestBody(413, request.contentLength, receivedBodySize);
			return;
		}
		if (_bodyExceedsLimit(limit))
		{
			Logger::error("Client fd " + toString(_fd)
				+ " request body too large");
			_prepareErrorResponse(413);
			return;
		}
	}

	if (status == HttpRequestParser::PARSE_ERROR)
	{
		Logger::error("Client fd " + toString(_fd) + " malformed request");
		_prepareErrorResponse(parser.getErrorStatusCode());
		return;
		}

	if (status == HttpRequestParser::PARSE_INCOMPLETE)
	{
		if (parser.headersComplete())
			_state = CLIENT_READING_BODY;
		return;
	}

	_state = CLIENT_PROCESSING;

	prepareResponse(RequestHandler::handle(parser.getRequest(), _context));
}

std::size_t ClientConnection::_getClientMaxBodySize() const
{
	if (_selectedServer)
		return _selectedServer->getClientMaxBodySize();
	if (_context.serverCandidates && !_context.serverCandidates->empty())
		return ((*_context.serverCandidates)[0].getClientMaxBodySize());

	return (DEFAULT_CLIENT_MAX_BODY_SIZE);
}

bool	ClientConnection::_bodyExceedsLimit(std::size_t limit) const
{
	std::size_t headerEnd = _readBuffer.find("\r\n\r\n");

	if (headerEnd == std::string::npos)
		return false;
	const std::size_t bodyStart = headerEnd + 4;
	if (_readBuffer.size() <= bodyStart)
		return false;
		
	return _readBuffer.size() - bodyStart > limit;
}

void ClientConnection::_rejectRequestBody(
	int code,
	std::size_t expectedBodySize,
	std::size_t receivedBodySize)
{
	if (receivedBodySize >= expectedBodySize)
		_discardBodyBytesRemaining = 0;
	else
		_discardBodyBytesRemaining = expectedBodySize - receivedBodySize;

	_readBuffer.clear();
	_prepareErrorResponse(code);
	if (_discardBodyBytesRemaining > 0)
		_state = CLIENT_REJECTING_BODY;
}

void ClientConnection::_prepareErrorResponse(int code)
{
	const ServerConfig* server = _selectedServer;

	if (!server && _context.serverCandidates
		&& !_context.serverCandidates->empty())
	{
		// Intentional default-server fallback before Host selection is possible.
		server = &(*_context.serverCandidates)[0];
	}
	prepareResponse(ErrorResponseFactory::create(code, server));
}
