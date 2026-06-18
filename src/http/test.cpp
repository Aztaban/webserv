#include <iostream>
#include "http/HttpRequestParser.hpp"
#include "http/HttpResponseBuilder.hpp"
#include "http/HttpRequestHandler.hpp"

int main()
{
	std::string raw =
		"POST /upload HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"Content-Length: 5\r\n"
		"\r\n"
		"hello";

	// Parse
	HttpRequestParser parser;
	HttpRequestParser::ParseStatus status = parser.parse(raw);

	if (status == HttpRequestParser::PARSE_INCOMPLETE)
	{
		std::cout << "[ERROR] Request incomplete" << std::endl;
		return 1;
	}

	if (status == HttpRequestParser::PARSE_ERROR)
	{
		std::cout << "[ERROR] Parse failed. HTTP "
				  << parser.getErrorStatusCode()
				  << std::endl;
		return 1;
	}

	std::cout << "[OK] Request parsed successfully" << std::endl;

	// Show parsed request
	const HttpRequest& req = parser.getRequest();

	std::cout << "Method: " << req.method << std::endl;
	std::cout << "Target: " << req.target << std::endl;
	std::cout << "Version: " << req.version << std::endl;
	std::cout << "Body: " << req.body << std::endl;

	// Handle request
	HttpRequestHandler handler;
	HttpResponse res = handler.handle(req);

	// Build HTTP response
	std::string output =
		HttpResponseBuilder::buildSimpleResponse(
			res.statusCode,
			res.reasonPhrase,
			res.body);

	std::cout << "\n===== GENERATED RESPONSE =====\n";
	std::cout << output << std::endl;

	return 0;
}