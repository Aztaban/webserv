#include "core/RequestHandler.hpp"
#include "http/HttpRequestHandler.hpp"

static std::string selectRootFromContext(const RequestContext& context)
{
    if (context.serverCandidates && !context.serverCandidates->empty())
        return ((*context.serverCandidates)[0].getRoot());

    /*
        TEMPORARY FALLBACK:
        Used until EventLoop/ClientConnection pass real RequestContext.
        Remove after TODO 5 / TODO 7 are finished.
    */
    return ("./www/site1");
}

HttpResponse RequestHandler::handle(const HttpRequest& request,
                                    const RequestContext& context)
{
    /*
        RequestHandler is the integration point.

        Current temporary flow:
        - choose root
        - call HttpRequestHandler directly

        Later final flow:
        - choose ServerConfig from context.serverCandidates
        - match LocationConfig with Router
        - check allowed methods
        - handle redirect/static/upload/delete/CGI
        - return HttpResponse
    */

    std::string root = selectRootFromContext(context);

    HttpRequestHandler handler(root);
    return (handler.handle(request));
}

//TEMPORARY COMPATIBILITY.
// // Do not use in ClientConnection.
// // Remove after tests are updated to use RequestHandler::handle().
// std::string RequestHandler::handleRawRequest(const std::string& rawRequest)
// {
// 	HttpRequestParser parser;

// 	HttpRequestParser::ParseStatus status = parser.parse(rawRequest);

// 	// ---------------- ERROR: incomplete request ----------------
// 	if (status == HttpRequestParser::PARSE_INCOMPLETE)
// 	{
// 		return HttpResponseBuilder::buildSimpleResponse(
// 			400,
// 			"Bad Request",
// 			"Incomplete request");
// 	}

// 	// ---------------- ERROR: invalid request ----------------
// 	if (status == HttpRequestParser::PARSE_ERROR)
// 	{
// 		return HttpResponseBuilder::buildSimpleResponse(
// 			parser.getErrorStatusCode(),
// 			"Error",
// 			"Malformed HTTP request");
// 	}

// 	// ---------------- VALID REQUEST ----------------
// 	RequestContext	context;
// 	HttpResponse	response = RequestHandler::handle(parser.getRequest(), context);

// 	return HttpResponseBuilder::build(response);
// }

