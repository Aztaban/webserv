#include "routing/Router.hpp"
#include <iostream>

// Select the server handling this request.
const ServerConfig* Router::selectServer(const HttpRequest& request, const RequestContext& context)
{
	(void)request;

	if (!context.serverCandidates
		|| context.serverCandidates->empty())
	{
		return NULL;
	}

	/*
	 * TODO:
	 * Improve later:
	 * - compare Host header
	 * - match server_name
	 *
	 * Current behavior:
	 * - first server for this listener
	 */
	return &(*context.serverCandidates)[0];
}

// Selects the most specific location for the request path.
const LocationConfig* Router::selectLocation(const HttpRequest& request, const ServerConfig& server)
{
	const LocationConfig* best = NULL;
	size_t bestLength = 0;

	const std::string& path = request.path;

	std::cout << "DEBUG request path: "
			  << path
			  << std::endl;

	for (size_t i = 0; i < server.getLocations().size(); i++)
	{
		const LocationConfig& location =
			server.getLocations()[i];

		const std::string& locationPath =
			location.path;

		std::cout << "DEBUG checking location: "
				  << locationPath
				  << std::endl;

		bool matches = false;

		if (locationPath == "/")
		{
			matches = true;
		}
		else if (path == locationPath)
		{
			matches = true;
		}
		else if (path.find(locationPath + "/") == 0)
		{
			matches = true;
		}

		if (matches)
		{
			std::cout << "DEBUG MATCH: "
					  << locationPath
					  << std::endl;

			if (locationPath.size() >= bestLength)
			{
				best = &location;
				bestLength = locationPath.size();
			}
		}
	}

	if (best)
	{
		std::cout << "DEBUG SELECTED LOCATION: "
				  << best->path
				  << std::endl;

		std::cout << "DEBUG SELECTED ROOT: "
				  << best->root
				  << std::endl;
	}
	else
	{
		std::cout << "DEBUG NO LOCATION FOUND"
				  << std::endl;
	}

	return best;
}

// Routes request to server and location.
RouteResult Router::route(
	const HttpRequest& request,
	const RequestContext& context)
{
	RouteResult result;

	result.server = selectServer(
		request,
		context);

	if (!result.server)
		return result;

	result.location = selectLocation(
		request,
		*result.server);

	return result;
}
