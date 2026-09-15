#include "routing/Router.hpp"

#include <iostream>
#include <cstdlib>
#include <string>

/*
** Webserv Unit Tests
**
** Target:
**     Router
**
** Planned tests:
**     - exact match
**     - longest prefix match
**     - missing route
**     - method restrictions
**     - upload route
**     - CGI route
**
** Status:
**     Placeholder only.
*/

static void	require(bool condition, const std::string& message)
{
	if (!condition)
	{
		std::cerr << "[FAIL]" << message << std::endl;
		std::exit(1);
	}
}

static void  test_exact_match(void)
{
	ServerConfig server("test", "127.0.0.1", 8000);

	LocationConfig root;
	root.path = "/";

	LocationConfig cgiBin;
	cgiBin.path = "/cgi-bin";

	server.addLocation(root);
	server.addLocation(cgiBin);

	HttpRequest request;
	request.path = "/cgi-bin";

	Router router;
	const LocationConfig* result = router.matchLocation(request, server);

	require(result != NULL, "exact match on /cgi-bin should return a location");
	require(result->path == "/cgi-bin", "exact match should return the /cgi-bin location, not /");
}

static void  test_longest_prefix_match(void)
{
	ServerConfig server("test", "127.0.0.1", 8000);

	LocationConfig root;
	root.path = "/";

	LocationConfig cgiBin;
	cgiBin.path = "/cgi-bin";

	server.addLocation(root);
	server.addLocation(cgiBin);

	HttpRequest request;
	request.path = "/cgi-bin/test.py";

	Router router;
	const LocationConfig* result = router.matchLocation(request, server);

	require(result != NULL, "/cgi-bin/test.py should match a location");
	require(result->path == "/cgi-bin", "the more specific /cgi-bin should win over /");
}

static void  test_root_fallback(void)
{
	ServerConfig server("test", "127.0.0.1", 8000);

	LocationConfig root;
	root.path = "/";

	LocationConfig cgiBin;
	cgiBin.path = "/cgi-bin";

	server.addLocation(root);
	server.addLocation(cgiBin);

	HttpRequest request;
	request.path = "/about.html";

	Router router;
	const LocationConfig* result = router.matchLocation(request, server);

	require(result != NULL, "/about.html should fall back to the root location");
	require(result->path == "/", "unmatched path should be served by /, not /cgi-bin");
}

static void  test_missing_route(void)
{
	ServerConfig server("test", "127.0.0.1", 8000);

	LocationConfig upload;
	upload.path = "/upload";

	server.addLocation(upload);

	HttpRequest request;
	request.path = "/nonexistent";

	Router router;
	const LocationConfig* result = router.matchLocation(request, server);

	require(result == NULL, "a path matching no location (no root defined) should return NULL");
}

static void  test_upload_route(void)
{
	ServerConfig server("test", "127.0.0.1", 8000);

	LocationConfig root;
	root.path = "/";

	LocationConfig upload;
	upload.path = "/upload";
	upload.uploadEnabled = true;
	upload.uploadPath = "./www/uploads";

	server.addLocation(root);
	server.addLocation(upload);

	HttpRequest request;
	request.path = "/upload/report.pdf";

	Router router;
	const LocationConfig* result = router.matchLocation(request, server);

	require(result != NULL, "/upload/report.pdf should match a location");
	require(result->path == "/upload", "should match /upload, not /");
	require(result->uploadEnabled == true, "matched location should carry uploadEnabled through");
}

static void  test_cgi_route(void)
{
	ServerConfig server("test", "127.0.0.1", 8000);

	LocationConfig root;
	root.path = "/";

	LocationConfig cgiBin;
	cgiBin.path = "/cgi-bin";
	cgiBin.cgiEnabled = true;
	cgiBin.cgiExtensions[".py"] = "/usr/bin/python3";

	server.addLocation(root);
	server.addLocation(cgiBin);

	HttpRequest request;
	request.path = "/cgi-bin/test.py";

	Router router;
	const LocationConfig* result = router.matchLocation(request, server);

	require(result != NULL, "/cgi-bin/test.py should match a location");
	require(result->path == "/cgi-bin", "should match /cgi-bin, not /");
	require(result->cgiEnabled == true, "matched location should carry cgiEnabled through");
	require(result->cgiExtensions.find(".py") != result->cgiExtensions.end(),
		"matched location should still have its .py interpreter mapping");
}

int main(void)
{
	test_exact_match();
	test_longest_prefix_match();
	test_root_fallback();
	test_missing_route();
	test_upload_route();
	test_cgi_route();

   	std::cout << "[PASS] Router unit tests" << std::endl;
	return (0);
}