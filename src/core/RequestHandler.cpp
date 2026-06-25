#include "core/RequestHandler.hpp"
#include "http/HttpRequestParser.hpp"
#include "http/HttpRequestHandler.hpp"
#include "http/HttpResponseBuilder.hpp"
#include "routing/Router.hpp"

std::string RequestHandler::handleRawRequest(const std::string& rawRequest)
{
	HttpRequestParser parser;

	HttpRequestParser::ParseStatus status = parser.parse(rawRequest);

	if (status == HttpRequestParser::PARSE_INCOMPLETE)
	{
		HttpResponse res;
		res.statusCode = 400;
		res.reasonPhrase = "Bad Request";
		res.body = "Incomplete request";
		res.headers["Content-Type"] = "text/plain";
		return HttpResponseBuilder::buildSimpleResponse(res);
	}

	if (status == HttpRequestParser::PARSE_ERROR)
	{
		HttpResponse res;
		res.statusCode = parser.getErrorStatusCode();
		res.reasonPhrase = "Error";
		res.body = "Malformed HTTP request";
		res.headers["Content-Type"] = "text/plain";
		return HttpResponseBuilder::buildSimpleResponse(res);
	}

	const HttpRequest& req = parser.getRequest();

	HttpRequestHandler handler("./www");
	HttpResponse res = handler.handle(req);

	return HttpResponseBuilder::buildSimpleResponse(res);
}

