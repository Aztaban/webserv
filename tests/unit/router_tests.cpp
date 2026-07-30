#include "routing/Router.hpp"

#include <cassert>
#include <vector>

static LocationConfig makeLocation(const std::string& path)
{
	LocationConfig location;
	location.path = path;
	return location;
}

static HttpRequest makeRequest(const std::string& path, const std::string& host)
{
	HttpRequest request;
	request.method = "GET";
	request.path = path;
	request.target = path;
	request.version = "HTTP/1.1";
	if (!host.empty())
		request.headers["hOsT"] = host;
	return request;
}

static void testVirtualServerSelection()
{
	std::vector<ServerConfig> servers(2);
	servers[0].addServerName("one.test");
	servers[1].addServerName("Two.Test");
	RequestContext context(&servers, "127.0.0.1", 8000);

	RouteResult route = Router::route(makeRequest("/", "  TWO.TEST:8000\t"), context);
	assert(route.errorStatusCode == 0);
	assert(route.server == &servers[1]);

	route = Router::route(makeRequest("/", "unknown.test"), context);
	assert(route.errorStatusCode == 0);
	assert(route.server == &servers[0]);

	route = Router::route(makeRequest("/", "two.test:not-a-port"), context);
	assert(route.errorStatusCode == 400);
	assert(route.server == &servers[0]);

	route = Router::route(makeRequest("/", ""), context);
	assert(route.errorStatusCode == 400);
	assert(route.server == &servers[0]);
}

static void testLocationSelection()
{
	std::vector<ServerConfig> servers(1);
	servers[0].addLocation(makeLocation("/"));
	servers[0].addLocation(makeLocation("/images"));
	servers[0].addLocation(makeLocation("/images/private"));
	RequestContext context(&servers, "127.0.0.1", 8000);

	RouteResult route = Router::route(makeRequest("/images/private/a.png", "site.test"), context);
	assert(route.location == &servers[0].getLocations()[2]);

	route = Router::route(makeRequest("/images/a.png", "site.test"), context);
	assert(route.location == &servers[0].getLocations()[1]);

	route = Router::route(makeRequest("/images2", "site.test"), context);
	assert(route.location == &servers[0].getLocations()[0]);
}

static void testTrailingSlashAndEqualLengthBehavior()
{
	std::vector<ServerConfig> servers(1);
	servers[0].addLocation(makeLocation("/foo/"));
	servers[0].addLocation(makeLocation("/foo"));
	RequestContext context(&servers, "127.0.0.1", 8000);

	RouteResult route = Router::route(makeRequest("/foo/bar", "site.test"), context);
	assert(route.location == &servers[0].getLocations()[0]);
}

static void testServerLevelFallback()
{
	std::vector<ServerConfig> servers(1);
	RequestContext context(&servers, "127.0.0.1", 8000);
	RouteResult route = Router::route(makeRequest("/anything", "site.test"), context);

	assert(route.server == &servers[0]);
	assert(route.location == NULL);
}

int main()
{
	testVirtualServerSelection();
	testLocationSelection();
	testTrailingSlashAndEqualLengthBehavior();
	testServerLevelFallback();
	return 0;
}
