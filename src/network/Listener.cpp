#include "network/Listener.hpp"
#include "utils/Logger.hpp"

#include <stdexcept>
#include <cstring>

#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

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
	if (fcntl(fd, F_SETFD, FD_CLOEXEC) == -1)
		throw std::runtime_error("fcntl(FD_CLOEXEC) failed");
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

	std::memset(&addr, 0, sizeof(addr));

	addr.sin_family = AF_INET;
	addr.sin_port = htons(_port);

	if (_host == "0.0.0.0")
		addr.sin_addr.s_addr = INADDR_ANY;
	else
		addr.sin_addr.s_addr = inet_addr(_host.c_str());
	if (addr.sin_addr.s_addr == INADDR_NONE)
		throw std::runtime_error("invalid host address");

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
		
	Logger::info("Accepted client from " + std::string(inet_ntoa(clientAddr.sin_addr)) + ":" + toString(ntohs(clientAddr.sin_port)));

	return (clientFd);
}
