#include "core/WebServer.hpp"
#include "utils/Logger.hpp"

#include <stdexcept>
#include <sstream>
#include <map>

static std::string	makeListenKey(const std::string& host, int port)
{
	std::ostringstream	ss;

	ss << host << ":" << port;
	return ss.str();
}

WebServer::WebServer(const std::vector<ServerConfig>& configs) : _configs(configs) {}

WebServer::~WebServer()
{
	for (std::size_t i = 0; i < _listeners.size(); i++)
		delete _listeners[i];
}

void	WebServer::_createListeners()
{
	if (_configs.empty())
		throw std::runtime_error("No server configuration provided");

	std::map<std::string, std::vector<ServerConfig> > groupedConfigs;

	for (std::size_t i = 0; i < _configs.size(); i++)
	{
		std::string	key = makeListenKey(_configs[i].getHost(), _configs[i].getPort());
		groupedConfigs[key].push_back(_configs[i]);
	}

	for (std::map<std::string, std::vector<ServerConfig> >::iterator it = groupedConfigs.begin();
		it != groupedConfigs.end(); it++)
	{
		const std::vector<ServerConfig>& candidates = it->second;

		Listener *listener = new Listener(
			candidates[0].getHost(),
			candidates[0].getPort(),
			candidates
		);

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
	_createListeners();
}

void WebServer::run()
{
	Logger::info("Starting server event loop");
	_eventLoop.run();
}