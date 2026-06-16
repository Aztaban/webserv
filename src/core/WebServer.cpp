#include "core/WebServer.hpp"
#include "utils/Logger.hpp"

#include <stdexcept>
#include <sstream>

WebServer::WebServer(const std::vector<ServerConfig>& configs) : _configs(configs) {}

WebServer::~WebServer()
{
	for (std::size_t i = 0; i < _listeners.size(); i++)
		delete _listeners[i];
}

void	WebServer::createListeners()
{
	if (_configs.empty())
		throw std::runtime_error("No server configuration provided");

	for (std::size_t i = 0; i < _configs.size(); i++)
	{
		Listener *listener = new Listener(_configs[i].getHost(), _configs[i].getPort());

		try {
			listener->setup();
			_eventLoop.addListener(listener);
			_listeners.push_back(listener);
		}
		catch (...){
			delete listener;
			throw;
		}
	}
}

void WebServer::setup()
{
	createListeners();
}

void WebServer::run()
{
	Logger::info("Starting server event loop");
	_eventLoop.run();
}