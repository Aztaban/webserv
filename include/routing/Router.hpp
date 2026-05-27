#ifndef ROUTER_HPP
# define ROUTER_HPP

# include "http/HttpRequest.hpp"

class Router {


public:
    Router();
    /**
     * Later
     * - receives HttpRequest + loaded ServerConfig list
     * - choose matching server/location
     * - checks allowed methods
     * - decides static / upload / delete / cgi / redirect 
     */
};

#endif