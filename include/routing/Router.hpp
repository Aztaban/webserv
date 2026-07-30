#ifndef ROUTER_HPP
#define ROUTER_HPP

#include <string>

#include "http/HttpRequest.hpp"
#include "core/RequestContext.hpp"
#include "config/ServerConfig.hpp"
#include "config/LocationConfig.hpp"

// Result of server and location selection.
struct RouteResult
{
	const	ServerConfig *server;
	const	LocationConfig *location;
	int		errorStatusCode;

	RouteResult()
			: server(NULL),
				location(NULL),
				errorStatusCode(0)
	{}
};

class Router
{
public:
	// Finds the server and location for a request.
	static RouteResult route(
			const HttpRequest &request,
			const RequestContext &context);

	// Shared Host-based virtual-server selection for request handling and
	// early body-limit/error paths. A malformed Host sets errorStatusCode=400.
	static const ServerConfig* selectServerFromHost(
		const std::string& hostHeader,
		const RequestContext& context,
		int& errorStatusCode);

private:
	// Selects matching server configuration.
	static const ServerConfig *selectServer(
			const HttpRequest &request,
			const RequestContext &context,
			int& errorStatusCode);

	// Selects the longest matching location.
	static const LocationConfig *selectLocation(
			const HttpRequest &request,
			const ServerConfig &server);
};

#endif