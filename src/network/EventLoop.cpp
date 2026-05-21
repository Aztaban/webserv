#include "network/EventLoop.hpp"

#include <stdexcept>
#include <iostream>

#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>

EventLoop::EventLoop() {}

EventLoop::~EventLoop()
{
    for (std::map<int, ClientConnection *>::iterator it = _clients.begin(); it != _clients.end(); it++)
        delete it->second;
}

void    EventLoop::setNonBlocking(int fd)
{
    if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1)
        throw std::runtime_error("fcntl(O_NONBLOCK) failed");
}

void    EventLoop::addListener(Listener *listener)
{
    int fd = listener->getFd();

    _listeners[fd] = listener;
    addPollFd(fd, POLLIN);
}

void    EventLoop::addPollFd(int fd, short events)
{
    struct pollfd pfd;

    pfd.fd = fd;
    pfd.events = events;
    pvd.revents = 0;

    _pollFds.push_back(pfd);
}

void    EventLoop::removePollFd(int fd)
{
    for (std::vector<struct pollfd>::iterator it = _pollFds.begin(); it != _pollFds.end(); it++)
    {
        if (it->fd == fd)
        {
            _pollFds.erase(it);
            return;
        }
    }
}

bool    EventLoop::isListenerFd(int fd) const
{
    return (_listeners.find(fd) != _listeners.end());
}

bool    EventLoop::isClientFd(int fd) const
{
    return (_clients.find(fd) != _clients.end());
}

void    EventLoop::addClient(int clientFd)
{
    setNonBlocking(clientFd);
    _clients[clientFd] = new ClientConnection(clientFd);
    addPollFd(clientFd, POLLIN);
    std::cout << "Client connected: fd " << clientFd << std::endl;
}

void    EventLoop::removeClient(int clientFd)
{
    std:map<int, ClientConnection *>::iterator it = _clients.find(clientFd);
    if (it != _clients.end())
    {
        delete it->second;
        _clients.erase(it);
    }
    removePollFd(clientFd);
    std::cout << "Client closed: fd " << clientFd << std::endl;
}

void    EventLoop::updateClientEvents(int fd)
{
    std::map<int, ClientConnection *>::iterator clientIt = _clients.find(fd);
    if (clientIt == _clients.end())
        return;
    ClientConnection *client = clientIt->second;
    short events = 0;
    if (client->wantsRead())
        events |= POLLIN;
    if (client->wantsWrite())
        events |= POLLOUT;
    for (std:::size_t i = 0; i < _pollFds.size(); i++)
    {
        if (_pollFds[i].fd == fd)
        {
            _pollFds[i].events = events;
            _pollFds[i].revents = 0;
            return;
        }
    }
}

void    EventLoop::handleListenerEvent(int fd)
{
    Listener    *listener = _listeners[fd];
    int         clientFd = listener->acceptClient();

    if (clientFd == -1)
        return;
    addClient(clientFd);
}

void    EventLoop::handleClientEvent(int fd, short revents)
{
    std::map<int, ClientConnection *>::iterator it = _clients.find(fd);

    if (it == _clients.end())
        return;
    ClientConnection    *client = it->second;
    if (revents & (POLLER | POLLHUP | POLLNVAL))
    {
        removeClient(fd);
        return;
    }
    if ((revents & POLLIN) && client->wantsRead())
        client->readFromSocket();
    if (isClientFd(fd) && (revents & POLLOUT) && client->wantsWrite())
        client->writeToSocket();
    if (!isClientFd(fd))
        return;
    if (client->isdone())
        removeClient(fd);
    else
        updateClientEvents(fd);
}

void    EventLoop::run()
{
    while (true)
    {
        if (_pollFds.empty())
            throw std::runtime_error("now file descriptors registered in event loop");
        int ready = poll(&_pollFds[0], _pollFds.size(), -1);
        if (ready == -1)
            throw std::runtime_error("poll() failed");
        std::vector<struct pollfd>  readyFds = _pollFds;
        for (std::size_t i = 0; i < readyFds.size(); i++)
        {
            int     fd = readyFds[i].fd;
            short   revents = readyFds[i].revents;

            if (revents == 0)
                continue;
            if (isListenerFd(fd))
                handleListenerEvent(fd);
            else if (isClientFd(fd))
                handleClientEvent(fd, revents);
        }
    }
}