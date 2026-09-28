#include "network/EventLoop.hpp"
#include "utils/Logger.hpp"
#include "core/RequestContext.hpp"

#include <stdexcept>
#include <iostream>
#include <ctime>
#include <vector>

#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>

EventLoop::EventLoop() {}

EventLoop::~EventLoop()
{
	while (!_clients.empty())
		_removeClient(_clients.begin()->first);
	for (std::size_t i = 0; i < _retiredCgiJobs.size(); ++i)
		delete _retiredCgiJobs[i];
}

void	EventLoop::_setNonBlocking(int fd)
{
	if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1)
		throw std::runtime_error("fcntl(O_NONBLOCK) failed");
	if (fcntl(fd, F_SETFD, FD_CLOEXEC) == -1)
		throw std::runtime_error("fcntl(FD_CLOEXEC) failed");
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

	try
	{
		_setNonBlocking(clientFd);
	}
	catch (const std::exception& error)
	{
		close(clientFd);
		Logger::error(error.what());
		return;
	}
	_clients[clientFd] = new ClientConnection(clientFd, context);
	_addPollFd(clientFd, POLLIN);

	Logger::info("Client connected: fd " + toString(clientFd));
}

void	EventLoop::_removeClient(int clientFd)
{
	_releaseCgi(clientFd);
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

	_cgiByClientFd[fd] = job;
	try
	{
		job->start(cgiRequest);
	}
	catch (...)
	{
		_releaseCgi(fd);
		throw;
	}
	if (job->hasFailed())
	{
		client->prepareErrorResponse(500);
		_releaseCgi(fd);
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
	}
}

void	EventLoop::_eraseFdFrom(int fd)
{
	std::map<int, int>::iterator clientIt = _cgiFdToClientFd.find(fd);
	if (clientIt != _cgiFdToClientFd.end())
	{
		_cgiFdToClientFd.erase(clientIt);
	}
	_cgiByFd.erase(fd);
}

void	EventLoop::_handleJobEvent(int fd, short revents)
{
	CgiJob *job = _cgiByFd.find(fd)->second;
	int jobStdin = job->getStdinFd();
	int jobStdout = job->getStdoutFd();

	if (revents & (POLLERR | POLLNVAL))
		job->killProcess();
	else if (fd == jobStdin && (revents & POLLOUT))
		job->writeToCgi();
	else if (fd == jobStdout && (revents & POLLIN))
		job->readFromCgi();
	else if (revents & (POLLERR | POLLHUP | POLLNVAL))
	{
		if (fd == jobStdin)
		{
			if (job->wantsWrite())
				job->killProcess();
			job->closeStdin();
		}
		else if (fd == jobStdout)
			job->closeStdout();
	}
	if (!job->wantsWrite())
		job->closeStdin();
	// Remove closed pipes immediately, even while the child is still running.
	if (jobStdin != -1 && job->getStdinFd() == -1)
	{
		_removePollFd(jobStdin);
		_eraseFdFrom(jobStdin);
	}
	if (jobStdout != -1 && job->getStdoutFd() == -1)
	{
		_removePollFd(jobStdout);
		_eraseFdFrom(jobStdout);
	}
}

void	EventLoop::_releaseCgi(int clientFd)
{
	std::map<int, CgiJob *>::iterator it = _cgiByClientFd.find(clientFd);
	if (it == _cgiByClientFd.end())
		return;
	CgiJob *job = it->second;
	_cgiByClientFd.erase(it);
	int stdinFd = job->getStdinFd();
	int stdoutFd = job->getStdoutFd();
	_removePollFd(stdinFd);
	_removePollFd(stdoutFd);
	_eraseFdFrom(stdinFd);
	_eraseFdFrom(stdoutFd);
	job->closeStdin();
	job->closeStdout();
	if (job->getPid() > 0)
		job->killProcess();
	// SIGKILL does not guarantee an immediately waitable child.
	if (job->getPid() > 0)
		_retiredCgiJobs.push_back(job);
	else
		delete job;
}

void	EventLoop::_checkCgiProcesses()
{
	for (std::vector<CgiJob *>::iterator it = _retiredCgiJobs.begin();
		it != _retiredCgiJobs.end();)
	{
		(*it)->updateProcessState();
		if ((*it)->isDone())
		{
			delete *it;
			it = _retiredCgiJobs.erase(it);
		}
		else
			++it;
	}
	for (std::map<int, CgiJob *>::iterator it = _cgiByClientFd.begin();
		it != _cgiByClientFd.end();)
	{
		int clientFd = it->first;
		CgiJob *job = it->second;
		++it;
		job->updateProcessState();
		if (job->hasFailed() || job->isDone())
		{
			_clients.find(clientFd)->second->prepareResponse(job->buildResponse());
			_releaseCgi(clientFd);
			_updateClientEvents(clientFd);
		}
	}
}

void	EventLoop::run()
{
	Logger::info("Event loop started");

	while (true)
	{
		if (_pollFds.empty())
			throw std::runtime_error("no file descriptors registered in event loop");
		_checkCgiProcesses();
		int timeout = (_cgiByClientFd.empty() && _retiredCgiJobs.empty()) ? 1000 : 100;
		int ready = poll(&_pollFds[0], _pollFds.size(), timeout);
		if (ready == -1)
			throw std::runtime_error("poll() failed");
		// CGI deadlines must prepare their response before idle-client cleanup.
		_removeTimedOutCgi();
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
			{
				if (revents & POLLIN)
					_handleListenerEvent(fd);
			}
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
		if (it->second->getState() != CLIENT_WAITING_CGI
			&& it->second->isTimedOut(now, CLIENT_TIMEOUT_SECONDS))
			timedOutFds.push_back(it->first);
	}

	for (std::size_t i = 0; i < timedOutFds.size(); i++)
	{
		Logger::warn("Client timed out: fd " + toString(timedOutFds[i]));
		_removeClient(timedOutFds[i]);
	}
}

void	EventLoop::_timeoutCgi(int clientFd)
{
	ClientConnection* client = _clients.find(clientFd)->second;
	// Preserve the selected server's error-page configuration and reset activity.
	client->prepareErrorResponse(504);
	_releaseCgi(clientFd);
	_updateClientEvents(clientFd);
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
		Logger::warn("CGI timed out: client fd " + toString(timedOutCgis[i]));
		_timeoutCgi(timedOutCgis[i]);
	}
}