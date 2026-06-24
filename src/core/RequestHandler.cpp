#include "core/RequestHandler.hpp"
#include "http/HttpRequestParser.hpp"
#include "http/HttpRequestHandler.hpp"
#include "http/HttpResponseBuilder.hpp"
#include "routing/Router.hpp"

HttpResponse	RequestHandler::handle(const HttpRequest& request, const RequestContext& context)
{
	(void)context;
	/*
		TEMPORARY:
		Direct handler with hardcoded root.

		Later:
		- choose ServerConfig from context.serverCandidates
		- match LocationConfig with Router
		- check methods
		- call static/upload/delete/cgi handler
	*/
	HttpRequestHandler handler("./www");
	return handler.handle(request);
}

std::string RequestHandler::handleRawRequest(const std::string& rawRequest)
{
	HttpRequestParser parser;

	HttpRequestParser::ParseStatus status = parser.parse(rawRequest);

	// ---------------- ERROR: incomplete request ----------------
	if (status == HttpRequestParser::PARSE_INCOMPLETE)
	{
		return HttpResponseBuilder::buildSimpleResponse(
			400,
			"Bad Request",
			"Incomplete request");
	}

	// ---------------- ERROR: invalid request ----------------
	if (status == HttpRequestParser::PARSE_ERROR)
	{
		return HttpResponseBuilder::buildSimpleResponse(
			parser.getErrorStatusCode(),
			"Error",
			"Malformed HTTP request");
	}

	// ---------------- VALID REQUEST ----------------
	RequestContext	context;
	HttpResponse	response = RequestHandler::handle(parser.getRequest(), context);

	return HttpResponseBuilder::build(response);
}

