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
	const ServerConfig *server;
	const LocationConfig *location;

	RouteResult()
			: server(NULL),
				location(NULL)
	{
	}
};

class Router
{
public:
	// Finds the server and location for a request.
	static RouteResult route(
			const HttpRequest &request,
			const RequestContext &context);

private:
	// Selects matching server configuration.
	static const ServerConfig *selectServer(
			const HttpRequest &request,
			const RequestContext &context);

	// Selects the longest matching location.
	static const LocationConfig *selectLocation(
			const HttpRequest &request,
			const ServerConfig &server);
};

#endif