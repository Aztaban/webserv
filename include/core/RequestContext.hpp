#ifndef REQUESTCONTEXT_HPP
# define REQUESTCONTEXT_HPP

# include "config/ServerConfig.hpp"

# include <string>
# include <vector>

/**
 * @brief Runtime context for one client request.
 *
 * RequestContext is created from the listener that accepted the client.
 * It tells RequestHandler which local host/port received the request and
 * which ServerConfig candidates belong to that listener.
 */
class RequestContext {

public:
	const std::vector<ServerConfig>*	serverCandidates;
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