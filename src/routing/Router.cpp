#include "routing/Router.hpp"

// Select the server handling this request.
const ServerConfig* Router::selectServer(
	const HttpRequest& request,
	const RequestContext& context)
{
	(void)request;

	if (!context.serverCandidates
		|| context.serverCandidates->empty())
	{
		return NULL;
	}

	return &(*context.serverCandidates)[0];
}

// Selects the most specific location for the request path.
const LocationConfig* Router::selectLocation(
	const HttpRequest& request,
	const ServerConfig& server)
{
	const LocationConfig* best = NULL;
	size_t bestLength = 0;

	std::string path = request.path;

	for (size_t i = 0; i < server.getLocations().size(); i++)
	{
		const LocationConfig& location =
			server.getLocations()[i];


		if (path.compare(
				0,
				location.path.size(),
				location.path) == 0)
		{
			if (location.path.size() >= bestLength)
			{
				best = &location;
				bestLength = location.path.size();
			}
		}
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
