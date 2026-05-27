#include "network/ClientConnection.hpp"
#include "utils/Logger.hpp"
#include "core/RequestHandler.hpp"

#include <sstream>

#include <unistd.h>
#include <sys/socket.h>

ClientConnection::ClientConnection(int fd) : _fd(fd),
                                             _state(CLIENT_READING_HEADERS),
                                             _lastActivity(std::time(NULL)),
                                             _headerEnd(std::string::npos),
                                             _contentLength(0)
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
    return (_state == CLIENT_READING_HEADERS || _state == CLIENT_READING_BODY); // Later parser state will decide if we need more request data
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
    char buffer[4096];
    ssize_t bytes = recv(_fd, buffer, sizeof(buffer), 0);

    if (bytes <= 0) // no errno check after recv()
    {
        Logger::debug("Client fd " + toString(_fd) + " disconnected during read");
        _state = CLIENT_DONE;
        return;
    }
    touch();

    Logger::debug("Client fd " + toString(_fd) + " read " + toString(bytes) + " byte(s)");

    _readBuffer.append(buffer, bytes);

    // TEMP: parser should handle max header size later
    if (_state == CLIENT_READING_HEADERS && _readBuffer.size() > MAX_HEADER_SIZE)
    {
        Logger::error("Client fd " + toString(_fd) + " header too large");
        prepareErrorResponse(400);
        _state = CLIENT_WRITING;
        return;
    }
    // TEMP: config/parser should handle max body size later
    if (_readBuffer.size() > MAX_HEADER_SIZE + MAX_BODY_SIZE)
    {
        Logger::error("Client fd " + toString(_fd) + " request too large");
        prepareErrorResponse(413);
        _state = CLIENT_WRITING;
        return;
    }
    // TEMP: replace with _parser.feed(buffer, bytes)
    processReadBuffer();
}

void ClientConnection::writeToSocket()
{
    ssize_t bytes;

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

void ClientConnection::touch()
{
    _lastActivity = std::time(NULL);
}

bool ClientConnection::isTimedOut(std::time_t now, int timeoutSeconds) const
{
    return (now - _lastActivity >= timeoutSeconds);
}

std::size_t ClientConnection::getBodyBytesReceived() const
{
    if (_headerEnd == std::string::npos) // TEMP: remove when parser tracks body progress
        return (0);
    if (_readBuffer.size() <= _headerEnd)
        return (0);
    return (_readBuffer.size() - _headerEnd);
}

void ClientConnection::parseTempContentLength()
{
    std::istringstream iss;
    std::size_t pos;
    std::size_t end;
    std::string value;

    _contentLength = 0;
    // TEMP: replace with real header parsing
    pos = _readBuffer.find("Content-Length:");
    if (pos == std::string::npos)
        return;
    pos += std::string("Content-Length:").size();
    while (pos < _readBuffer.size() && (_readBuffer[pos] == ' ' || _readBuffer[pos] == '\t'))
        pos++;
    end = _readBuffer.find("\r\n", pos);
    if (end == std::string::npos)
        return;

    value = _readBuffer.substr(pos, end - pos);
    iss.str(value);
    iss >> _contentLength;

    if (iss.fail())
    {
        Logger::error("Client fd " + toString(_fd) + " invalid Content-Length");
        prepareErrorResponse(400);
        _state = CLIENT_WRITING;
    }
}

// TEMP: replace this whole function with HttpRequestParser integration
void ClientConnection::processReadBuffer()
{
    if (_state == CLIENT_READING_HEADERS)
    {
        std::size_t pos;

        pos = _readBuffer.find("\r\n\r\n");
        if (pos == std::string::npos)
            return;
        _headerEnd = pos + 4;
        Logger::debug("Client fd " + toString(_fd) + " headers complete");

        parseTempContentLength();

        if (_state == CLIENT_WRITING)
            return;
        if (_contentLength > MAX_BODY_SIZE) // TEMP: max body size should come from matched server/location config
        {
            Logger::error("Client fd " + toString(_fd) + " body too large");
            prepareErrorResponse(413);
            _state = CLIENT_WRITING;
            return;
        }
        if (_contentLength > 0)
            _state = CLIENT_READING_BODY;
        else
            _state = CLIENT_PROCESSING;
    }

    if (_state == CLIENT_READING_BODY)
    {
        if (getBodyBytesReceived() < _contentLength)
            return;
        Logger::debug("Client fd " + toString(_fd) + " body complete");
        _state = CLIENT_PROCESSING;
    }

    if (_state == CLIENT_PROCESSING)
    {
        // PLUG HERE: Router + HttpResponseBuilder
        _writeBuffer = RequestHandler::handleRawRequest(_readBuffer);
        _state = CLIENT_WRITING;
        Logger::debug("Client fd " + toString(_fd) + " switched to CLIENT_WRITING");
    }
}

void ClientConnection::prepareErrorResponse(int code)
{
    // TEMP: later use default/custom error pages from config
    std::string status;

    if (code == 400)
        status = "400 Bad Request";
    else if (code == 413)
        status = "413 Payload Too Large";
    else if (code == 500)
        status = "500 Internal Server Error";
    else
        status = "500 Internal Server Error";

    std::string body = "<html><body><h1>" + status + "</h1></body></html>";

    std::ostringstream length;
    length << body.size();

    _writeBuffer =
        "HTTP/1.1 " + status + "\r\n"
        "Content-Type: text/html\r\n"
        "Content-Length: ";

    _writeBuffer += length.str();
    _writeBuffer += "\r\n";
    _writeBuffer += "Connection: close\r\n";
    _writeBuffer += "\r\n";
    _writeBuffer += body;
}

// TEMP: parser should own this check later
bool ClientConnection::headersComplete() const
{
    return (_readBuffer.find("\r\n\r\n") != std::string::npos);
}