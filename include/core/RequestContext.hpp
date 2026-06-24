#ifndef REQUESTCONTEXT_HPP
# define REQUESTCONTEXT_HPP

# include "config/ServerConfig.hpp"

# include <string>
# include <vector>

class RequestContext {

public:
    const std::vector<ServerConfig>*    serverCandidates;
    std::string localHost;
    int localPort;

    RequestContext() : serverCandidates(NULL), localHost(), localPort(0) {}

    RequestContext(const std::vector<ServerConfig>* candidates, const std::string& host, int port)
        : serverCandidates(candidates),
          localHost(host),
          localPort(port)
    {
    }
};

#endif