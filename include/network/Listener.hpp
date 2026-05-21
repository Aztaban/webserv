#ifndef LISTENER_HPP
# define LISTENER_HPP

# include <string>

class Listener {

private:
    std::string _host;
    int         _port;
    int         _fd;

    void    setNonBlocking(int fd);

public:
    Listener(const std::string& host, int port);
    ~Listener();

    void    setup()
    int     getFd() const;
    int     getPort() const;

    int     acceptClient() const;
};

#endif


