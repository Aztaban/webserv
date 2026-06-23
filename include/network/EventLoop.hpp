#ifndef EVENTLOOP_HPP
# define EVENTLOOP_HPP

# include "network/Listener.hpp"
# include "network/ClientConnection.hpp"

# include <vector>
# include <map>

# include <poll.h>

class EventLoop {

private:
    static const int    CLIENT_TIMEOUT_SECONDS = 30;

    std::vector<struct pollfd>          _pollFds;
    std::map<int, Listener *>           _listeners;
    std::map<int, ClientConnection *>   _clients;

    bool    _isListenerFd(int fd) const;
    bool    _isClientFd(int fd) const;
    
    void    _addPollFd(int fd, short events);
    void    _removePollFd(int fd);
    void    _updateClientEvents(int fd);
    void    _handleListenerEvent(int fd);
    void    _handleClientEvent(int fd, short revents);
    void    _addClient(int clientFd);
    void    _removeClient(int clientFd);
    void    _setNonBlocking(int fd);
    void    _removeTimedOutClients();

public:
    EventLoop();
    ~EventLoop();

    void    addListener(Listener *listener);
    void    run();

};

#endif