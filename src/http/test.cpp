#include <iostream>
#include <vector>

#include "http/HttpRequestParser.hpp"
#include "http/HttpResponseBuilder.hpp"
#include "http/HttpRequestHandler.hpp"

static void runTest(const std::string& raw, HttpRequestHandler& handler)
{
	HttpRequestParser parser;
	HttpRequestParser::ParseStatus status = parser.parse(raw);

	std::cout << "\n=============================\n";
	std::cout << "RAW REQUEST:\n" << raw << "\n";

	if (status == HttpRequestParser::PARSE_INCOMPLETE)
	{
		std::cout << "[ERROR] Incomplete request\n";
		return;
	}

	if (status == HttpRequestParser::PARSE_ERROR)
	{
		std::cout << "[ERROR] Parse failed. HTTP "
				  << parser.getErrorStatusCode() << "\n";
		return;
	}

	const HttpRequest& req = parser.getRequest();

	std::cout << "[OK] Parsed\n";
	std::cout << "Method: " << req.method << "\n";
	std::cout << "Target: " << req.target << "\n";
	std::cout << "Body: " << req.body << "\n";

	HttpResponse res = handler.handle(req);

	std::string output =
		HttpResponseBuilder::buildSimpleResponse(
			res.statusCode,
			res.reasonPhrase,
			res.body);

	std::cout << "\n--- RESPONSE ---\n";
	std::cout << output << "\n";
}

int main()
{
	HttpRequestHandler handler("/var/www");

	std::vector<std::string> tests;

	tests.push_back("GET / HTTP/1.1\r\nHost: localhost\r\n\r\n");

	tests.push_back(
		"POST /upload HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"Content-Length: 5\r\n\r\n"
		"hello"
	);

	tests.push_back("DELETE /file HTTP/1.1\r\nHost: localhost\r\n\r\n");

	tests.push_back("FOO / HTTP/1.1\r\nHost: localhost\r\n\r\n");

	for (size_t i = 0; i < tests.size(); i++)
		runTest(tests[i], handler);

	return 0;
}