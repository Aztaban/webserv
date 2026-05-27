#ifndef WEBSERVER_HPP
# define WEBSERVER_HPP

# include "config/ServerConfig.hpp"
# include "network/Listener.hpp"
# include "network/EventLoop.hpp"

# include <vector>

class WebServer {

private:
    ServerConfig            _config;
    std::vector<Listener *> _listeners;
    EventLoop               _eventLoop;

    void    createListeners();

public:
    WebServer(const ServerConfig& config);
    ~WebServer();

    void    setup();
    void    run();

};

#endif