#include "http/HttpResponseBuilder.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

static void require(bool condition, const std::string& message)
{
	if (!condition)
	{
		std::cerr << "[FAIL] " << message << std::endl;
		std::exit(1);
	}
}

static bool contains(const std::string& text, const std::string& needle)
{
	return (text.find(needle) != std::string::npos);
}

static std::size_t countOccurrences(const std::string& text, const std::string& needle)
{
	std::size_t count = 0;
	std::size_t pos = 0;

	while (true)
	{
		pos = text.find(needle, pos);
		if (pos == std::string::npos)
			break;
		++count;
		pos += needle.size();
	}
	return (count);
}

static void test_builds_status_line_and_body(void)
{
	HttpResponse response;
	response.statusCode = 200;
	response.reasonPhrase = "OK";
	response.body = "hello";

	std::string raw = HttpResponseBuilder::build(response);

	require(raw.find("HTTP/1.1 200 OK\r\n") == 0,
		"response should start with valid status line");
	require(contains(raw, "\r\n\r\nhello"),
		"response should contain CRLF CRLF separator before body");
}

static void test_adds_content_length(void)
{
	HttpResponse response;
	response.body = "hello";

	std::string raw = HttpResponseBuilder::build(response);

	require(contains(raw, "Content-Length: 5\r\n"),
		"response should include calculated Content-Length");
}

static void test_adds_default_content_type(void)
{
	HttpResponse response;
	response.body = "plain text";

	std::string raw = HttpResponseBuilder::build(response);

	require(contains(raw, "Content-Type: text/plain\r\n"),
		"response should include default text/plain Content-Type");
}

static void test_keeps_custom_content_type_once(void)
{
	HttpResponse response;
	response.body = "<h1>Hello</h1>";
	response.headers["Content-Type"] = "text/html";

	std::string raw = HttpResponseBuilder::build(response);

	require(contains(raw, "Content-Type: text/html\r\n"),
		"response should include custom Content-Type");
	require(countOccurrences(raw, "Content-Type:") == 1,
		"response should not duplicate Content-Type");
}

static void test_connection_header(void)
{
	HttpResponse response;
	response.body = "hello";
	response.closeConnection = false;

	std::string raw = HttpResponseBuilder::build(response);

	require(contains(raw, "Connection: keep-alive\r\n"),
		"closeConnection=false should produce keep-alive header");
}

static void test_simple_response(void)
{
	std::string raw = HttpResponseBuilder::buildSimpleResponse(
		404,
		"Not Found",
		"missing"
	);

	require(raw.find("HTTP/1.1 404 Not Found\r\n") == 0,
		"simple response should use provided status line");
	require(contains(raw, "Content-Length: 7\r\n"),
		"simple response should calculate Content-Length");
	require(contains(raw, "\r\n\r\nmissing"),
		"simple response should append body");
}

int main(void)
{
	test_builds_status_line_and_body();
	test_adds_content_length();
	test_adds_default_content_type();
	test_keeps_custom_content_type_once();
	test_connection_header();
	test_simple_response();

	std::cout << "[PASS] HttpResponseBuilder unit tests" << std::endl;
	return (0);
}
