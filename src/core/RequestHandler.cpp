#include "core/RequestHandler.hpp"
#include "core/ErrorResponseFactory.hpp"

// Creates the HTTP handler using the selected route.
static HttpRequestHandler createHttpHandler(const RouteResult& route)
{
	std::string root;
	std::string locationPath;
	std::string uploadDirectory;
	std::string index;

	std::vector<std::string> methods;
	std::map<int, std::string> errorPages;

	bool autoindex = false;
	size_t maxBodySize = 0;

	if (route.location)
	{
		root = route.location->root;
		locationPath = route.location->path;
		uploadDirectory = route.location->uploadPath;
		index = route.location->index;
		methods = route.location->methods;
		autoindex = route.location->autoindex;
	}
	if (route.server)
	{
		if (root.empty())
			root = route.server->getRoot();

		if (index.empty())
			index = route.server->getIndex();

		maxBodySize =
			route.server->getClientMaxBodySize();

		errorPages =
			route.server->getErrorpages();
	}
	if (locationPath.empty())
		locationPath = "/";

	return HttpRequestHandler(
		root,
		locationPath,
		uploadDirectory,
		index,
		methods,
		errorPages,
		autoindex,
		maxBodySize);
}

// Main request entry point.
HttpResponse RequestHandler::handle(const HttpRequest& request,const RequestContext& context)
{
	RouteResult route = Router::route(request, context);

	if (route.errorStatusCode != 0)
	{
		const std::string root = route.server ? route.server->getRoot() : "";
		return ErrorResponseFactory::create(route.errorStatusCode, root);
	}

	if (!route.server)
	{
		Logger::error(
			"RequestHandler: no matching server");

		return HttpResponse(
			500,
			"Internal Server Error",
			"No server configuration");
	}

	HttpRequestHandler handler = createHttpHandler(route);

	return handler.handle(request);
}
