#include "network/EventLoop.hpp"
#include "utils/Logger.hpp"
#include "core/RequestContext.hpp"

#include <stdexcept>
#include <iostream>
#include <ctime>
#include <vector>

#include <fcntl.h>
#include <sys/socket.h>

EventLoop::EventLoop() {}

EventLoop::~EventLoop()
{
	for (std::map<int, ClientConnection *>::iterator it = _clients.begin(); it != _clients.end(); it++)
		delete it->second;
}

void	EventLoop::_setNonBlocking(int fd)
{
	if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1)
		throw std::runtime_error("fcntl(O_NONBLOCK) failed");
}

void	EventLoop::addListener(Listener *listener)
{
	int	fd = listener->getFd();

	_listeners[fd] = listener;
	_addPollFd(fd, POLLIN);
	Logger::debug("Registered listener fd " + toString(fd) + " in poll");
}

void	EventLoop::_addPollFd(int fd, short events)
{
	struct pollfd	pfd;

	pfd.fd = fd;
	pfd.events = events;
	pfd.revents = 0;

	_pollFds.push_back(pfd);
}

void	EventLoop::_removePollFd(int fd)
{
	for (std::vector<struct pollfd>::iterator it = _pollFds.begin(); it != _pollFds.end(); it++)
	{
		if (it->fd == fd)
		{
			_pollFds.erase(it);
			return;
		}
	}
}

bool	EventLoop::_isListenerFd(int fd) const
{
	return (_listeners.find(fd) != _listeners.end());
}

bool	EventLoop::_isClientFd(int fd) const
{
	return (_clients.find(fd) != _clients.end());
}

bool	EventLoop::_isCgiFd(int fd) const
{
	return (_cgiByFd.find(fd) != _cgiByFd.end());
}

void	EventLoop::_addClient(int clientFd, Listener* listener)
{
	RequestContext	context(
		&(listener->getServerCandidates()),
		listener->getHost(),
		listener->getPort()
	);

	_setNonBlocking(clientFd);
	_clients[clientFd] = new ClientConnection(clientFd, context);
	_addPollFd(clientFd, POLLIN);

	Logger::info("Client connected: fd " + toString(clientFd));
}

void	EventLoop::_removeClient(int clientFd)
{
	std::map<int, CgiJob *>::iterator jobIt = _cgiByClientFd.find(clientFd);
	if (jobIt != _cgiByClientFd.end())
	{
		CgiJob *job = jobIt->second;
		int jobStdin = job->getStdinFd();
		int jobStdout = job->getStdoutFd();

		job->killProcess();
		_removePollFd(jobStdin);
		_removePollFd(jobStdout);
		_eraseFdFrom(jobStdin);
		_eraseFdFrom(jobStdout);
		delete job;
	}
	std::map<int, ClientConnection *>::iterator	it = _clients.find(clientFd);
	_removePollFd(clientFd);
	if (it != _clients.end())
	{
		delete it->second;
		_clients.erase(it);
	}


	Logger::info("Client closed: fd " + toString(clientFd));
}

void	EventLoop::_updateClientEvents(int fd)
{
	std::map<int, ClientConnection *>::iterator	clientIt = _clients.find(fd);
	if (clientIt == _clients.end())
		return;
	ClientConnection*	client = clientIt->second;
	short	events = 0;
	if (client->wantsRead())
		events |= POLLIN;
	if (client->wantsWrite())
		events |= POLLOUT;
	for (std::size_t i = 0; i < _pollFds.size(); i++)
	{
		if (_pollFds[i].fd == fd)
		{
			_pollFds[i].events = events;
			_pollFds[i].revents = 0;
			Logger::debug("Updated client fd " + toString(fd) + " events to " + toString(events));
			return;
		}
	}
}

void	EventLoop::_handleListenerEvent(int fd)
{
	Logger::debug("Listener fd " + toString(fd) + " is readable");

	std::map<int, Listener *>::iterator	it = _listeners.find(fd);
	if (it == _listeners.end())
		return;
		
	Listener*	listener = it->second;
	while (true)
	{
		int clientFd = listener->acceptClient();
		if (clientFd == -1)
			return;
		_addClient(clientFd, listener);
	}
}

void	EventLoop::_handleClientEvent(int fd, short revents)
{
	Logger::debug("Client fd " + toString(fd) + " event: " + toString(revents));

	std::map<int, ClientConnection *>::iterator	it = _clients.find(fd);

	if (it == _clients.end())
		return;
	ClientConnection*	client = it->second;
	if (revents & (POLLERR | POLLHUP | POLLNVAL))
	{
		_removeClient(fd);
		return;
	}
	if ((revents & POLLIN) && client->wantsRead())
		client->readFromSocket();
	if (client->getState() == CLIENT_WAITING_CGI)
	{
		_startCgiForClient(fd, client);
	}
	if (client->isDone())
	{
		_removeClient(fd);
		return;
	}
	if ((revents & POLLOUT) && client->wantsWrite())
		client->writeToSocket();
	if (client->isDone())
	{
		_removeClient(fd);
		return;
	}
	_updateClientEvents(fd);
}

void EventLoop::_startCgiForClient(int fd, ClientConnection* client)
{
	CgiRequest	cgiRequest = client->getCgiRequest();
	CgiJob *job = new CgiJob();

	job->start(cgiRequest);
	if (job->hasFailed())
	{
		client->prepareResponse(job->buildResponse());
		delete job;
	}
	else
	{
		_addPollFd(job->getStdoutFd(), POLLIN);
		_cgiFdToClientFd[job->getStdoutFd()] = fd;
		_cgiByFd[job->getStdoutFd()] = job;
		if (job->wantsWrite() == true)
		{
			_addPollFd(job->getStdinFd(), POLLOUT);
			_cgiFdToClientFd[job->getStdinFd()] = fd;
			_cgiByFd[job->getStdinFd()] = job;
		}
		else
			job->closeStdin();
		_cgiByClientFd[client->getFd()] = job;
	}
}

void	EventLoop::_eraseFdFrom(int fd)
{
	std::map<int, int>::iterator clientIt = _cgiFdToClientFd.find(fd);
	if (clientIt != _cgiFdToClientFd.end())
	{
		_cgiByClientFd.erase(clientIt->second);
		_cgiFdToClientFd.erase(clientIt);
	}
	_cgiByFd.erase(fd);
}

void	EventLoop::_handleJobEvent(int fd, short revents)
{
	CgiJob *job = _cgiByFd[fd];
	ClientConnection *client = _clients[_cgiFdToClientFd[fd]];
	int jobStdin = job->getStdinFd();
	int jobStdout = job->getStdoutFd();

	if (fd == job->getStdinFd() && (revents & POLLOUT))
		job->writeToCgi();
	else if (fd == job->getStdoutFd() && (revents & POLLIN))
		job->readFromCgi();
	else if (revents & (POLLERR | POLLHUP | POLLNVAL))
	{
		if (fd == job->getStdinFd())
			job->closeStdin();
		else if (fd == job->getStdoutFd())
			job->closeStdout();
	}
	if (!job->wantsWrite() && fd == jobStdin && jobStdin != -1)
	{
		job->closeStdin();
		_removePollFd(jobStdin);
		_eraseFdFrom(jobStdin);
	}
	if (job->isDone())
	{
		client->prepareResponse(job->buildResponse());
		_removePollFd(jobStdin);
		_removePollFd(jobStdout);
		_eraseFdFrom(jobStdin);
		_eraseFdFrom(jobStdout);
		if (job->getStdinFd() != -1)
			job->closeStdin();
		job->closeStdout();

		delete job;
		_updateClientEvents(client->getFd());
	}
}

void	EventLoop::run()
{
	Logger::info("Event loop started");

	while (true)
	{
		if (_pollFds.empty())
			throw std::runtime_error("no file descriptors registered in event loop");
		int ready = poll(&_pollFds[0], _pollFds.size(), 1000);
		if (ready == -1)
			throw std::runtime_error("poll() failed");
		_removeTimedOutClients();
		if (ready == 0)
			continue;

		Logger::debug("poll() returned " + toString(ready) + " ready fd(s)");

		std::vector<struct pollfd>	readyFds = _pollFds;
		for (std::size_t i = 0; i < readyFds.size(); i++)
		{
			int	fd = readyFds[i].fd;
			short	revents = readyFds[i].revents;

			if (revents == 0)
				continue;
			if (_isListenerFd(fd))
				_handleListenerEvent(fd);
			else if (_isClientFd(fd))
				_handleClientEvent(fd, revents);
			else if (_isCgiFd(fd))
				_handleJobEvent(fd, revents);
		}
	}
}

void	EventLoop::_removeTimedOutClients()
{
	std::time_t			now = std::time(NULL);
	std::vector<int>	timedOutFds;

	for (std::map<int, ClientConnection *>::iterator it = _clients.begin(); it != _clients.end(); it++)
	{
		if (it->second->isTimedOut(now, CLIENT_TIMEOUT_SECONDS))
			timedOutFds.push_back(it->first);
	}

	for (std::size_t i = 0; i < timedOutFds.size(); i++)
	{
		Logger::warn("Client timed out: fd " + toString(timedOutFds[i]));
		_removeClient(timedOutFds[i]);
	}
}

void	EventLoop::_removeCgi(int clientFd)
{
	ClientConnection* client = _clients[clientFd];
	CgiJob* job = _cgiByClientFd.find(clientFd)->second;	
	int jobStdin = job->getStdinFd();
	int jobStdout = job->getStdoutFd();

	job->killProcess();
	client->prepareResponse(job->buildResponse());
	_removePollFd(jobStdin);
	_removePollFd(jobStdout);
	_eraseFdFrom(jobStdin);
	_eraseFdFrom(jobStdout);
	if (job->getStdinFd() != -1)
		job->closeStdin();
	job->closeStdout();
	delete job;
	_updateClientEvents(client->getFd());
}

void	EventLoop::_removeTimedOutCgi()
{
	std::time_t				now = std::time(NULL);
	std::vector<int>		timedOutCgis;

	for (std::map<int, CgiJob*>::iterator it = _cgiByClientFd.begin(); it != _cgiByClientFd.end(); it++)
	{
		if (it->second->isTimedOut(now, CGI_TIMEOUT_SECONDS))
			timedOutCgis.push_back(it->first);
	}
	
	for (std::size_t i = 0; i < timedOutCgis.size(); i++)
	{
		Logger::warn("CGI timed out: fd");
		_removeCgi(timedOutCgis[i]);
	}
}