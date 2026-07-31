#ifndef REQUESTHANDLER_HPP
# define REQUESTHANDLER_HPP

# include "core/EffectiveRouteConfig.hpp"
# include "core/RequestHandler.hpp"
# include "http/HttpRequestHandler.hpp"
# include "routing/Router.hpp"
# include "utils/Logger.hpp"

class RequestHandler
{
	public:
		// Merges the selected server and location into one source of truth for
		// redirect, CGI, and static request handling.
		static EffectiveRouteConfig buildEffectiveConfig(const RouteResult& route);
		// Handles one parsed request.
		static HttpResponse handle(
			const HttpRequest& request,
			const RequestContext& context);
};

#endif