#include "core/RequestHandler.hpp"
#include "core/ErrorResponseFactory.hpp"

namespace
{
	struct EffectiveRouteConfig
	{
		std::string root;
		std::string locationPath;
		std::string uploadDirectory;
		std::string index;
		std::vector<std::string> methods;
		std::map<int, std::string> errorPages;
		bool autoindex;
		bool uploadEnabled;
		std::size_t maxBodySize;

		EffectiveRouteConfig()
			: root(),
			  locationPath("/"),
			  uploadDirectory(),
			  index(),
			  methods(),
			  errorPages(),
			  autoindex(false),
			  uploadEnabled(false),
			  maxBodySize(0)
		{}
	};

	static EffectiveRouteConfig buildEffectiveConfig(const RouteResult& route)
	{
		EffectiveRouteConfig config;

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
		}
		return config;
	}

	static HttpRequestHandler createHttpHandler(const RouteResult& route)
	{
		const EffectiveRouteConfig config = buildEffectiveConfig(route);

		return HttpRequestHandler(
			config.root,
			config.locationPath,
			config.uploadDirectory,
			config.index,
			config.methods,
			config.errorPages,
			config.autoindex,
			config.uploadEnabled,
			config.maxBodySize);
	}
}

HttpResponse RequestHandler::handle(
	const HttpRequest& request,
	const RequestContext& context)
{
	RouteResult route = Router::route(request, context);

	if (route.errorStatusCode != 0)
		return ErrorResponseFactory::create(route.errorStatusCode, route.server);

	if (!route.server)
	{
		Logger::error("RequestHandler: no matching server");
		return ErrorResponseFactory::create(500, static_cast<const ServerConfig*>(NULL));
	}

	HttpRequestHandler handler = createHttpHandler(route);
	return handler.handle(request);
}
