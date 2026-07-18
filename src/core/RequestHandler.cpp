#include "core/RequestHandler.hpp"
#include <iostream>

// Creates the HTTP handler using the selected route.
static HttpRequestHandler createHttpHandler(const RouteResult& route)
{
	std::string root;
	std::string uploadDirectory;
	bool autoindex = false;
	size_t maxBodySize = 0;

	if (route.location)
	{
		root = route.location->root;
		uploadDirectory = route.location->uploadPath;
		autoindex = route.location->autoindex;
	}

	if (route.server)
	{
		if (root.empty())
			root = route.server->getRoot();

		maxBodySize = route.server->getClientMaxBodySize();
	}

	//std::cout << "DEBUG handler root: " << root << std::endl;

	return HttpRequestHandler(
		root,
		uploadDirectory,
		"",
		autoindex,
		maxBodySize);
}

// Main request entry point.
HttpResponse RequestHandler::handle(
	const HttpRequest& request,
	const RequestContext& context)
{
	RouteResult route = Router::route(request, context);

	if (!route.server)
	{
		Logger::error("RequestHandler: no matching server");
		return HttpResponse(
			500,
			"Internal Server Error",
			"No server configuration");
	}

	HttpRequestHandler handler = createHttpHandler(route);

	return handler.handle(request);
}
