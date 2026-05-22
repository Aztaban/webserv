#include "network/Listener.hpp"
#include <stdexcept>
#include <cstring>
#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

Listener::Listener(const std::string& host, int port) : _host(host), _port(port) {}

Listener::~Listener()
{
    if (_fd != -1)
        close(_fd);
}

void    Listener::setNonBlocking(int fd)
{
    if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1)
        throw std::runtime_error("fcntl(O_NONBLOCK) failed");
}

void    Listener::setup() 
{
    struct sockaddr_in addr;
    int opt = 1;

    _fd = socket(AF_INET, SOCK_STREAM, 0);
    if (_fd == -1)
        throw std::runtime_error("socket() failed");
    
    if (setsockopt(_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
        throw std::runtime_error("setsockopt(SO_REUSEADDR) failed");

    setNonBlocking(_fd);

    std::memset(&addr, 0, sizeof(addr));

    addr.sin_family = AF_INET;
    addr.sin_port = htons(_port);

    if (_host == "0.0.0.0")
        addr.sin_addr.s_addr = INADDR_ANY;
    else
        addr.sin_addr.s_addr = inet_addr(_host.c_str());

    if (bind(_fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) == -1)
        throw std::runtime_error("bind() failed");

    if (listen(_fd, SOMAXCONN) == -1)
        throw std::runtime_error("listen() failed");

    std::cout << "Listening on " << _host <<  ":" << _port << std::endl;
}

int Listener::getFd() const
{
    return (_fd);
}

int Listener::getPort() const
{
    return (_port);
}

int Listener::acceptClient() const
{
    struct      sockaddr_in clientAddr;
    socklen_t   clientLen = sizeof(clientAddr);

    int clientFd = accept(_fd, reinterpret_cast<struct sockaddr *>(&clientAddr), &clientLen);

    return (clientFd);
}