#include "core/WebServer.hpp"

WebServer::WebServer(const ServerConfig& config) : _config(config) {}

WebServer::~WebServer()
{
    for (std::size_t i = 0; i < _listeners.size(); i++)
        delete _listeners[i];
}

void    WebServer::createListeners()
{
    Listener    *listener = new Listener(_config.getHost(), _config.getPort());

    listener->setup();
    _listeners.push_back(listener);
    _eventLoop.addListener(listener);
}

void WebServer::setup()
{
    createListeners();
}

void WebServer::run()
{
    _eventLoop.run();
}