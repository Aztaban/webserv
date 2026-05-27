#include "core/WebServer.hpp"

WebServer::WebServer(const ServerConfig& config) : _config(config) {}

WebServer::~WebServer()
{
    for (std::size_t i = 0; i < _listeners.size(); i++)
        delete _listeners[i];
}

// TEMP: later loop over std::vector<ServerConfig>
void    WebServer::createListeners()
{
    // TEMP: later create listeners from parsed config server blocks
    Listener *listener1 = new Listener(_config.getHost(), _config.getPort());
    listener1->setup();
    _listeners.push_back(listener1);
    _eventLoop.addListener(listener1);

    Listener    *listener2 = new Listener("127.0.0.1", 8001);
    listener2->setup();
    _listeners.push_back(listener2);
    _eventLoop.addListener(listener2);
}

void WebServer::setup()
{
    createListeners();
}

void WebServer::run()
{
    _eventLoop.run();
}