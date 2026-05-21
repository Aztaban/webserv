#ifndef SERVERCONFIG_HPP
# define SERVERCONFIG_HPP

# include <string>

class ServerConfig {

private:
    std::string _host;
    int         _port;
    
public:
    ServerConfig();
    ServerConfig(const std::string &host, int port);

    const std::string& getHost() const;
    int getPort() const;

};

#endif