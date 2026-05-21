#ifndef EVENTLOOP_HPP
# define EVENTLOOP_HPP

# include "network/Listener.hpp"
# include "network/ClientConnection.hpp"
# include <vector>
# include <map>
# include <poll.h>

class EventLoop {

private:
    std::vector<struct pollfd>          _pollFds;
    std::map<int, Listener *>           _listeners;
    std::map<int, ClientConnection *>   _clients;

    bool    isListenerFd(int fd) const;
    bool    isClientFd(int fd) const;
    
    void    addPollFd(int fd, short events);
    void    removePollFd(int fd);
    void    updateClientEvents(int fd);
    void    handleListenerEvent(int fd);
    void    handleClientEvent(int fd, short revents);
    void    addClient(int clientFd);
    void    removeClient(int clientFd);
    void    setNonBlocking(int fd);

public:
    EventLoop();
    ~EventLoop();

    void    addListener(Listener *listener);
    void    run();

};