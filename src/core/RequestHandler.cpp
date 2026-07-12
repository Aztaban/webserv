#include "core/RequestHandler.hpp"
#include "http/HttpRequestHandler.hpp"
#include "utils/Logger.hpp"

static std::string selectRootFromContext(const RequestContext& context)
{
	if (!context.serverCandidates || context.serverCandidates->empty())
		return ("");

	return ((*context.serverCandidates)[0].getRoot());
}


HttpResponse RequestHandler::handle(
	const HttpRequest& request,
	const RequestContext& context)
{
	std::string root = selectRootFromContext(context);

	if (root.empty())
	{
		Logger::error("RequestHandler received empty RequestContext");
		return (HttpResponse(
			500,
			"Internal Server Error",
			"No server configuration"));
	}


	HttpRequestHandler handler(
		root,
		"",
		"",
		false,
		0);

	return (handler.handle(request));
}
