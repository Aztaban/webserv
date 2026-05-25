#include "network/ClientConnection.hpp"
#include "utils/Logger.hpp"

#include <sstream>

#include <unistd.h>
#include <sys/socket.h>

ClientConnection::ClientConnection(int fd) : _fd(fd), _state(CLIENT_READING) {}

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

bool    ClientConnection::wantsRead() const
{
    return (_state == CLIENT_READING);
}

bool    ClientConnection::wantsWrite() const
{
    return (_state == CLIENT_WRITING);
}

bool    ClientConnection::isDone() const
{
    return (_state == CLIENT_DONE);
}

void    ClientConnection::readFromSocket()
{
    char    buffer[4096];
    int     bytes = recv(_fd, buffer, sizeof(buffer), 0);

    if (bytes <= 0)
    {
        Logger::debug("Client fd " + toString(_fd) + " disconnected during read");
        _state = CLIENT_DONE;
        return;
    }

    Logger::debug("Client fd " + toString(_fd) + " read " + toString(bytes) + " byte(s)");
    
    _readBuffer.append(buffer, bytes);
    if (_readBuffer.find("\r\n\r\n") != std::string::npos)
    {
        Logger::debug("Client fd " + toString(_fd) + " headers complete");

        prepareHardcodeResponse();
        _state = CLIENT_WRITING;

        Logger::debug("Client fd " + toString(_fd) + " switched to WRITING_RESPONSE");
    }
}

void    ClientConnection::writeToSocket()
{
    if (_writeBuffer.empty())
    {
        _state = CLIENT_DONE;
        return;
    }
    int bytes = send(_fd, _writeBuffer.c_str(), _writeBuffer.size(), 0);
    if (bytes <= 0)
    {
        Logger::debug("Client fd " + toString(_fd) + " disconnected during write");
        _state = CLIENT_DONE;
        return;
    }
    
    Logger::debug("Client fd " + toString(_fd) + " sent " + toString(bytes) + " byte(s)");
    
    _writeBuffer.erase(0, bytes);
    if (_writeBuffer.empty())
        _state = CLIENT_DONE;
}

void ClientConnection::prepareHardcodeResponse()
{
    std::string body =
        "<!DOCTYPE html>\n"
        "<html>\n"
        "<head><title>webserv</title></head>\n"
        "<body>\n"
        "<h1>Hello from webserv</h1>\n"
        "<p>Network core is alive.</p>\n"
        "</body>\n"
        "</html>\n";

    std::ostringstream length;
    length << body.size();

    _writeBuffer =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Content-Length: ";

    _writeBuffer += length.str();
    _writeBuffer += "\r\n";
    _writeBuffer += "Connection: close\r\n";
    _writeBuffer += "\r\n";
    _writeBuffer += body;
}