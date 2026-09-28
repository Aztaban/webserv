#include "network/Listener.hpp"
#include "utils/Logger.hpp"

#include <stdexcept>
#include <cstring>

#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <netdb.h>
#include <sstream>

Listener::Listener(const std::string& host, int port) 
	: _host(host),
	  _port(port) ,
	  _fd(-1),
	  _serverCandidates()
{
}

Listener::Listener(const std::string& host, int port, const std::vector<ServerConfig>& serverCandidates)
	: _host(host),
	  _port(port) ,
	  _fd(-1),
	  _serverCandidates(serverCandidates)
{
}

Listener::~Listener()
{
	if (_fd != -1)
		close(_fd);
}

void	Listener::_setNonBlocking(int fd)
{
	if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1)
		throw std::runtime_error("fcntl(O_NONBLOCK) failed");
}

void	Listener::setup() 
{
	Logger::debug("Creating listening IPv4 socket");
	struct sockaddr_in	addr;
	int	opt = 1;

	_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (_fd == -1)
		throw std::runtime_error("socket() failed");
		
	if (setsockopt(_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
		throw std::runtime_error("setsockopt(SO_REUSEADDR) failed");

	Logger::debug("Setting listener fd " + toString(_fd) + " non-blocking");
	_setNonBlocking(_fd);

	struct addrinfo         hints;
    struct addrinfo         *result = NULL;
    std::ostringstream      port;

    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    port << _port;

    int status = getaddrinfo(_host.c_str(), port.str().c_str(), &hints, &result);

    if (status != 0)
            throw std::runtime_error(std::string("invalid host address: ") + gai_strerror(status));
    std::memcpy(&addr, result->ai_addr, sizeof(addr));
    freeaddrinfo(result);

	Logger::debug("Binding listener socket");
	if (bind(_fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) == -1)
		throw std::runtime_error("bind() failed");

	Logger::debug("Starting listen()");
	if (listen(_fd, SOMAXCONN) == -1)
		throw std::runtime_error("listen() failed");

	Logger::info("Listening on " + _host + ":" + toString(_port));
}

int	Listener::getFd() const
{
	return _fd;
}

int	Listener::getPort() const
{
	return _port;
}

const std::string&	Listener::getHost() const
{
	return _host;
}

const std::vector<ServerConfig>&	Listener::getServerCandidates() const
{
	return _serverCandidates;
}

int	Listener::acceptClient() const
{
	struct sockaddr_in	clientAddr;
	socklen_t	clientLen = sizeof(clientAddr);

	std::memset(&clientAddr, 0, sizeof(clientAddr));

	int clientFd = accept(_fd, reinterpret_cast<struct sockaddr *>(&clientAddr), &clientLen);
	if (clientFd == -1)
		return (-1);

	return (clientFd);
}
