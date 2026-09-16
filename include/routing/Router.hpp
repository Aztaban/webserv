#ifndef ROUTER_HPP
# define ROUTER_HPP

# include "http/HttpRequest.hpp"
# include "config/ServerConfig.hpp"
# include "config/LocationConfig.hpp"

class Router {

public:
    Router();
    ~Router();

    const ServerConfig* matchServer(const std::vector<ServerConfig>& servers, const HttpRequest& request) const;
    const LocationConfig* matchLocation(const HttpRequest& request, const ServerConfig& server) const;

};

#endif