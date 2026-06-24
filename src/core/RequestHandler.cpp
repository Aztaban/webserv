#include "core/RequestHandler.hpp"
#include "http/HttpRequestParser.hpp"
#include "http/HttpRequestHandler.hpp"
#include "http/HttpResponseBuilder.hpp"
#include "routing/Router.hpp"
#include "core/RequestContext.hpp"

HttpResponse	RequestHandler::handle(const HttpRequest& request, const RequestContext& context)
{
	std::string	root = "./www/site1";

	if (context.serverCandidates && !context.serverCandidates->empty())
		root = (*context.serverCandidates)[0].getRoot();
	
	HttpRequestHandler handler(root);
	return handler.handle(request);
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

