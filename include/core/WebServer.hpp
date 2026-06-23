#ifndef WEBSERVER_HPP
# define WEBSERVER_HPP

# include "config/ServerConfig.hpp"
# include "network/Listener.hpp"
# include "network/EventLoop.hpp"

# include <vector>

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