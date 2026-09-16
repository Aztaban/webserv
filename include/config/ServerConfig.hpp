#ifndef SERVERCONFIG_HPP
# define SERVERCONFIG_HPP

# include "config/LocationConfig.hpp"

# include <string>
# include <vector>
# include <map>
# include <cstddef>

class ServerConfig {

private:
	std::string _serverName;
    std::vector<std::string> _serverNames;
    std::string _host;
    int         _port;
    std::string _root;
    std::string _index;
    std::size_t _clientMaxBodySize;
    std::map<int, std::string> _errorPages;
    std::vector<LocationConfig> _locations;

public:
    ServerConfig();
    ServerConfig(const std::string& name, const std::string& host, int port);

	const std::string&	getName() const;
    const std::vector<std::string>& getServerNames() const;
    const std::string&  getHost() const;
    int                 getPort() const;

    const std::string&  getRoot() const;
    const std::string&  getIndex() const;
    std::size_t         getClientMaxBodySize() const;

    const std::map<int, std::string>&   getErrorpages() const;
    const std::vector<LocationConfig>&  getLocations() const;


	void    setName(const std::string& name);
    void    addServerName(const std::string& name);
    void	setHost(const std::string& host);
	void	setPort(int host);
    void    setRoot(const std::string& root);
    void    setIndex(const std::string& index);
    void    setClientMaxBodySize(std::size_t size);
    void    addErrorPage(int statusCode, const std::string& path);
    void    addLocation(const LocationConfig& location);
};

#endif