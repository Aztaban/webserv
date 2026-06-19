#include "core/RequestHandler.hpp"
#include "http/HttpRequestParser.hpp"
#include "http/HttpRequestHandler.hpp"
#include "http/HttpResponseBuilder.hpp"
#include "routing/Router.hpp"

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
	const HttpRequest& req = parser.getRequest();

	// TEMPORARY: direct handler (router comes later)
	HttpRequestHandler handler("./www");

	HttpResponse res = handler.handle(req);

	// ---------------- BUILD RESPONSE ----------------
	return HttpResponseBuilder::buildSimpleResponse(
		res.statusCode,
		res.reasonPhrase,
		res.body);
}

