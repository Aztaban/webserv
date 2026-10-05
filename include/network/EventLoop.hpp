#ifndef EVENTLOOP_HPP
# define EVENTLOOP_HPP

# include "cgi/CgiJob.hpp"
# include "network/Listener.hpp"
# include "network/ClientConnection.hpp"

# include <vector>
# include <map>

# include <poll.h>

/**
 * @brief Owns the main poll-based I/O loop.
 *
 * EventLoop registers listener sockets and client sockets in one poll()
 * instance. It accepts new clients, dispatches readable/writable client
 * events, updates requested events, and removes closed or timed-out clients.
 *
 * All socket read/write operations must happen through objects managed here
 * after poll() reports the fd as ready.
 */
class EventLoop {

private:
	static const int	CLIENT_TIMEOUT_SECONDS = 30;
	static const int	CGI_TIMEOUT_SECONDS = 10;
	static const std::size_t MAX_CONCURRENT_CGI = 4;

	std::vector<struct pollfd>			_pollFds;
	std::map<int, Listener *>			_listeners;
	std::map<int, ClientConnection *>	_clients;
	std::map<int, CgiJob *>				_cgiByFd;
	std::map<int, int>					_cgiFdToClientFd;
	std::map<int, CgiJob *>				_cgiByClientFd;

	std::vector<CgiJob *>	_retiredCgiJobs;

	bool	_isListenerFd(int fd) const;
	bool	_isClientFd(int fd) const;
	bool	_isCgiFd(int fd) const;

	void	_addPollFd(int fd, short events);
	void	_removePollFd(int fd);
	void	_updateClientEvents(int fd);
	void	_handleListenerEvent(int fd);
	void	_handleClientEvent(int fd, short revents);
	void	_handleJobEvent(int fd, short revents);
	void	_startCgiForClient(int fd, ClientConnection* client);
	void	_startQueuedCgi();
	void	_addClient(int clientFd, Listener* listener);
	void	_removeClient(int clientFd);
	void	_setNonBlocking(int fd);
	void	_removeTimedOutClients();
	void	_removeTimedOutCgi();
	void	_eraseFdFrom(int fd);
	void	_timeoutCgi(int clientFd);
	void	_releaseCgi(int clientFd);
	void	_checkCgiProcesses();
	void _recoverFromError(int fd);
	int		_highestFd() const;

public:
	EventLoop();
	~EventLoop();

	/**
	 * @brief Registers a listening socket in the poll loop.
	 */
	void	addListener(Listener *listener);

	/**
	 * @brief Starts the main server loop.
	 *
	 * This function does not return during normal server execution.
	 */
	void	run();

};

#endif