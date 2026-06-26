#ifndef WEBSERVER_HPP
# define WEBSERVER_HPP

# include "config/ServerConfig.hpp"
# include "network/Listener.hpp"
# include "network/EventLoop.hpp"

# include <vector>

/**
 * @brief Top-level server coordinator.
 *
 * WebServer owns the loaded ServerConfig objects, creates the listening
 * sockets, registers them in the EventLoop, and starts the main loop.
 *
 * It does not handle individual client I/O directly.
 */
class WebServer {

private:
	std::vector<ServerConfig>	_configs;
	std::vector<Listener *>		_listeners;
	EventLoop					_eventLoop;

	void	_createListeners();

public:
	WebServer(const std::vector<ServerConfig>& configs);
	~WebServer();

	void	setup();
	void	run();

};

#endif