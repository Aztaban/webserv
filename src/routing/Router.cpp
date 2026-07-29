#include "routing/Router.hpp"
#include "utils/Logger.hpp"

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

	Logger::debug("DEBUG request path: " + path);

	for (size_t i = 0; i < server.getLocations().size(); i++)
	{
		const LocationConfig& location =
			server.getLocations()[i];

		const std::string& locationPath =
			location.path;

		Logger::debug("DEBUG checking location: " + locationPath);

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
			Logger::debug("DEBUG MATCH: " + locationPath);

			if (locationPath.size() >= bestLength)
			{
				best = &location;
				bestLength = locationPath.size();
			}
		}
	}

	if (best)
		Logger::debug("Router: selected location " + best->path + " for " + path);
	else
		Logger::debug("Router: no location matched " + path);

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
