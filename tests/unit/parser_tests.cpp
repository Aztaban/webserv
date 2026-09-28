#include "http/HttpRequestParser.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

static void	require(bool condition, const std::string& message)
{
	if (!condition)
	{
		std::cerr << "[FAIL]" << message << std::endl;
		std::exit(1);
	}
}

static void test_valid_get_request(void)
{
	HttpRequestParser parser;
	std::string raw =
		"GET /index.html?name=webserv HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"Connection: close\r\n"
		"\r\n";

	require(parser.parse(raw) == HttpRequestParser::PARSE_COMPLETE,
		"valid GET should parse completely");

	const HttpRequest& request = parser.getRequest();

	require(request.method == "GET", "method should be GET");
	require(request.target == "/index.html?name=webserv", "target should keep query");
	require(request.path == "/index.html", "path should exclude query");
	require(request.query == "name=webserv", "query should be parsed");
	require(request.version == "HTTP/1.1", "version should be HTTP/1.1");
	require(request.headers.find("Host") != request.headers.end(), "Host header should exist");
	require(request.hasBody == false, "GET should not have body");
	require(request.contentLength == 0, "GET content length should be zero");
	require(parser.getConsumedBytes() == raw.size(), "consumed bytes should match request size");
}

static void test_incomplete_headers(void)
{
	HttpRequestParser parser;
	std::string raw =
		"GET / HTTP/1.1\r\n"
		"Host: localhost\r\n";

	require(parser.parse(raw) == HttpRequestParser::PARSE_INCOMPLETE,
		"request without CRLF CRLF should be incomplete");
}

static void test_valid_post_with_body(void)
{
	HttpRequestParser parser;
	std::string raw =
		"POST /upload/file.txt HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"Content-Length: 11\r\n"
		"\r\n"
		"hello world";

	require(parser.parse(raw) == HttpRequestParser::PARSE_COMPLETE,
		"valid POST should parse completely");

	const HttpRequest& request = parser.getRequest();

	require(request.method == "POST", "method should be POST");
	require(request.path == "/upload/file.txt", "POST path should be parsed");
	require(request.hasBody == true, "POST should have body");
	require(request.contentLength == 11, "content length should be parsed");
	require(request.body == "hello world", "body should be parsed");
}

static void test_short_body_is_incomplete(void)
{
	HttpRequestParser parser;
	std::string raw =
		"POST /upload/file.txt HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"Content-Length: 20\r\n"
		"\r\n"
		"short";

	require(parser.parse(raw) == HttpRequestParser::PARSE_INCOMPLETE,
		"body shorter than Content-Length should be incomplete");
}

static void test_malformed_header_returns_400(void)
{
	HttpRequestParser parser;
	std::string raw =
		"GET / HTTP/1.1\r\n"
		"Host localhost\r\n"
		"\r\n";

	require(parser.parse(raw) == HttpRequestParser::PARSE_ERROR,
		"malformed header should return parse error");
	require(parser.getErrorStatusCode() == 400,
		"malformed header should set error 400");
}

static void test_missing_host_returns_400(void)
{
	HttpRequestParser parser;
	std::string raw =
		"GET / HTTP/1.1\r\n"
		"Connection: close\r\n"
		"\r\n";

	require(parser.parse(raw) == HttpRequestParser::PARSE_ERROR,
		"HTTP/1.1 without Host should return parse error");
	require(parser.getErrorStatusCode() == 400,
		"missing Host should set error 400");
}

static void test_unsupported_method_returns_501(void)
{
	HttpRequestParser parser;
	std::string raw =
		"PUT /file.txt HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"\r\n";

	require(parser.parse(raw) == HttpRequestParser::PARSE_ERROR,
		"unsupported method should return parse error");
	require(parser.getErrorStatusCode() == 501,
		"unsupported method should set error 501");
}

int main(void)
{
	test_valid_get_request();
	test_incomplete_headers();
	test_valid_post_with_body();
	test_short_body_is_incomplete();
	test_malformed_header_returns_400();
	test_missing_host_returns_400();
	test_unsupported_method_returns_501();

	std::cout << "[PASS] HttpRequestParser unit tests" << std::endl;
	return (0);
}
