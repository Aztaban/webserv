#ifndef LISTENER_HPP
# define LISTENER_HPP

# include "config/ServerConfig.hpp"

# include <string>
# include <vector>

class Listener {

private:
    std::string _host;
    int         _port;
    int         _fd;
    std::vector<ServerConfig>   _serverCandidates;

    Listener(const Listener &other);
    Listener &operator=(const Listener &other);
    void    _setNonBlocking(int fd);

public:
    Listener(const std::string& host, int port);
    Listener(const std::string& host, int port, const std::vector<ServerConfig>& serverCandidates);
    ~Listener();

    void    setup();
    int     getFd() const;
    int     getPort() const;
    const std::string&  getHost() const;
    const std::vector<ServerConfig>&    getServerCandidates() const;

    int     acceptClient() const;
};

#endif


