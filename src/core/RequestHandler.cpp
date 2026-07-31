#include "core/RequestHandler.hpp"
#include "core/ErrorResponseFactory.hpp"
#include "http/HttpRequestHandler.hpp"
#include "utils/Logger.hpp"

namespace
{
	static HttpRequestHandler	createHttpHandler(const EffectiveRouteConfig& config)
	{
		return HttpRequestHandler(
			config.root,
			config.locationPath,
			config.uploadDirectory,
			config.index,
			config.methods,
			config.errorPages,
			config.autoindex,
			config.uploadEnabled,
			config.maxBodySize
		);
	}
}

EffectiveRouteConfig RequestHandler::buildEffectiveConfig(const RouteResult& route)
{
	EffectiveRouteConfig config;

	config.server = route.server;
	config.location = route.location;

	if (route.server)
	{
		config.root = route.server->getRoot();
		config.index = route.server->getIndex();
		config.errorPages = route.server->getErrorpages();
		config.maxBodySize = route.server->getClientMaxBodySize();
	}
	if (route.location)
	{
		config.locationPath = route.location->path;
		if (!route.location->root.empty())
			config.root = route.location->root;
		if (!route.location->index.empty())
			config.index = route.location->index;
		config.methods = route.location->methods;
		config.autoindex = route.location->autoindex;
		config.uploadEnabled = route.location->uploadEnabled;
		if (config.uploadEnabled)
			config.uploadDirectory = route.location->uploadPath;

		config.hasRedirect = route.location->hasRedirect;
		config.redirectCode = route.location->redirectCode;
		config.redirectUrl = route.location->redirectUrl;

		config.cgiEnabled = route.location->cgiEnabled;
		config.cgiExtensions = route.location->cgiExtensions;
	}
	return config;
}

HttpResponse RequestHandler::handle(
	const HttpRequest& request,
	const RequestContext& context)
{
	const RouteResult route = Router::route(request, context);

	if (route.errorStatusCode != 0)
		return ErrorResponseFactory::create(route.errorStatusCode, route.server);

	if (!route.server)
	{
		Logger::error("RequestHandler: no matching server");
		return ErrorResponseFactory::create(500, static_cast<const ServerConfig*>(NULL));
	}

	const EffectiveRouteConfig config = buildEffectiveConfig(route);

	// Stable partner integration order:
	// 1. redirect handler checks config.hasRedirect
	// 2. CGI handler checks config.cgiEnabled and the request path extension
	// 3. non-redirect/non-CGI requests continue to the static HTTP handler
	HttpRequestHandler handler = createHttpHandler(config);
	return handler.handle(request);
}
