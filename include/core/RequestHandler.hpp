#ifndef REQUESTHANDLER_HPP
# define REQUESTHANDLER_HPP

#include "core/RequestHandler.hpp"
#include "http/HttpRequestHandler.hpp"
#include "routing/Router.hpp"
#include "utils/Logger.hpp"

class RequestHandler
{
	public:
		// Handles one parsed request.
		static HttpResponse handle(
			const HttpRequest& request,
			const RequestContext& context);
};

#endif