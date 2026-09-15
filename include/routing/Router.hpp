#ifndef ROUTER_HPP
# define ROUTER_HPP

# include "http/HttpRequest.hpp"
# include "config/ServerConfig.hpp"
# include "config/LocationConfig.hpp"

class Router {

public:
    Router();
    ~Router();

    const LocationConfig* matchLocation(const HttpRequest& request, const ServerConfig& server) const;

};

#endif