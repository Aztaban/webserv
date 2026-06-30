#include "network/ClientConnection.hpp"
#include "utils/Logger.hpp"
#include "core/RequestHandler.hpp"
#include "http/HttpRequestParser.hpp"
#include "http/HttpResponseBuilder.hpp"

#include <unistd.h>
#include <sys/socket.h>

ClientConnection::ClientConnection(int fd, const RequestContext& context)
	: _fd(fd),
	  _state(CLIENT_READING_HEADERS),
	  _readBuffer(),
	  _writeBuffer(),
	  _lastActivity(std::time(NULL)),
	  _context(context)
{
}

static const std::size_t DEFAULT_CLIENT_MAX_BODY_SIZE = 1000000;

static std::string reasonPhrase(int code)
{
	if (code == 400)
		return ("Bad Request");
	if (code == 405)
		return ("Method Not Allowed");
	if (code == 413)
		return ("Payload Too Large");
	if (code == 431)
		return ("Request Header Fields Too Large");
	if (code == 501)
		return ("Not Implemented");
	if (code == 505)
		return ("HTTP Version Not Supported");
	return ("Internal Server Error");
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
	char	buffer[4096];
	ssize_t	bytes = recv(_fd, buffer, sizeof(buffer), 0);

	if (bytes <= 0) // no errno check after recv()
	{
		Logger::debug("Client fd " + toString(_fd) + " disconnected during read");
		_state = CLIENT_DONE;
		return;
	}
	touch();

	Logger::debug("Client fd " + toString(_fd) + " read " + toString(bytes) + " byte(s)");

	_readBuffer.append(buffer, bytes);

	if (_readBuffer.find("\r\n\r\n") == std::string::npos
		&& _readBuffer.size() > MAX_HEADER_SIZE)
	{
		Logger::error("Client fd " + toString(_fd) + " header too large");
		_prepareErrorResponse(431);
		_state = CLIENT_WRITING;
		return;
	}
	if (_bodyExceedsLimit())
	{
		Logger::error("Client fd " + toString(_fd) + " request too large");
		_prepareErrorResponse(413);
		_state = CLIENT_WRITING;
		return;
	}
	_processReadBuffer();
}

void ClientConnection::writeToSocket()
{
	ssize_t	bytes;

	if (_writeBuffer.empty())
	{
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
	if (_writeBuffer.empty())
		_state = CLIENT_DONE; // Later: reset to READING_HEADERS here if keep-alive is supported
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

void ClientConnection::_processReadBuffer()
{
	HttpRequestParser				parser;
	HttpRequestParser::ParseStatus	status;

	status = parser.parse(_readBuffer);
	if (status == HttpRequestParser::PARSE_INCOMPLETE)
		return;
		
	if (status == HttpRequestParser::PARSE_ERROR)
	{
		Logger::error("Client fd " + toString(_fd) + " malformed request");
		_prepareErrorResponse(parser.getErrorStatusCode());
		return;
	}

	_state = CLIENT_PROCESSING;

	HttpResponse response = RequestHandler::handle(parser.getRequest(), _context);
	prepareResponse(response);
}

std::size_t ClientConnection::_getClientMaxBodySize() const
{
	if (_context.serverCandidates && !_context.serverCandidates->empty())
		return ((*_context.serverCandidates)[0].getClientMaxBodySize());

	return (DEFAULT_CLIENT_MAX_BODY_SIZE);
}

bool	ClientConnection::_bodyExceedsLimit() const
{
	std::size_t	headerEnd;
	std::size_t	bodyStart;

	headerEnd = _readBuffer.find("\r\n\r\n");
	if (headerEnd == std::string::npos)
		return false;
	bodyStart = headerEnd + 4;
	if (_readBuffer.size() <= bodyStart)
		return false;
		
	return ((_readBuffer.size() - bodyStart) > _getClientMaxBodySize());
}


void ClientConnection::_prepareErrorResponse(int code)
{
	std::string		reason = reasonPhrase(code);
	HttpResponse	response;

	response.statusCode = code;
	response.reasonPhrase = reason;
	response.body = reason;
	response.headers["Content-Type"] = "text/plain";
	response.closeConnection = true;

	prepareResponse(response);
}
