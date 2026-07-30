#include "routing/Router.hpp"
#include "http/HttpHeaderUtils.hpp"

namespace
{
	static bool isDecimalPort(const std::string& value)
	{
		if (value.empty())
			return false;

		unsigned long port = 0;
		for (std::size_t i = 0; i < value.size(); i++)
		{
			if (value[i] < '0' || value[i] > '9')
				return false;
			port = port * 10 + static_cast<unsigned long>(value[i] - '0');
			if (port > 65535)
				return false;
		}
		return port > 0;
	}

	static bool normalizeHostValue(
		const std::string& hostHeader,
		std::string& normalizedHost)
	{
		std::string value = HttpHeaderUtils::trimOws(hostHeader);
		std::size_t colon;

		if (value.empty())
			return false;

		for (std::size_t i = 0; i < value.size(); i++)
		{
			const unsigned char ch = static_cast<unsigned char>(value[i]);
			if (std::isspace(ch) || std::iscntrl(ch)
				|| value[i] == '/' || value[i] == '\\'
				|| value[i] == '?' || value[i] == '#'
				|| value[i] == ',' || value[i] == '@')
				return false;
		}

		colon = value.find(':');
		if (colon != std::string::npos)
		{
			if (value.find(':', colon + 1) != std::string::npos)
				return false;
			if (colon == 0 || !isDecimalPort(value.substr(colon + 1)))
				return false;
			value.erase(colon);
		}

		if (value.empty())
			return false;
		normalizedHost = HttpHeaderUtils::lowercase(value);
		return true;
	}

	static std::string normalizedLocationPath(const std::string& value)
	{
		std::string path = value;

		while (path.size() > 1 && path[path.size() - 1] == '/')
			path.erase(path.size() - 1);
		return path;
	}

	static bool locationMatches(
		const std::string& requestPath,
		const std::string& locationPath)
	{
		if (locationPath == "/")
			return !requestPath.empty() && requestPath[0] == '/';
		if (requestPath == locationPath)
			return true;
		return requestPath.size() > locationPath.size()
			&& requestPath.compare(0, locationPath.size(), locationPath) == 0
			&& requestPath[locationPath.size()] == '/';
	}
}

const ServerConfig* Router::selectServerFromHost(
	const std::string& hostHeader,
	const RequestContext& context,
	int& errorStatusCode)
{
	errorStatusCode = 0;
	if (!context.serverCandidates || context.serverCandidates->empty())
		return NULL;

	const ServerConfig* defaultServer = &(*context.serverCandidates)[0];
	std::string normalizedHost;

	if (!normalizeHostValue(hostHeader, normalizedHost))
	{
		errorStatusCode = 400;
		return defaultServer;
	}

	for (std::size_t i = 0; i < context.serverCandidates->size(); i++)
	{
		const ServerConfig& server = (*context.serverCandidates)[i];
		const std::vector<std::string>& names = server.getServerNames();

		for (std::size_t j = 0; j < names.size(); j++)
		{
			if (HttpHeaderUtils::lowercase(names[j]) == normalizedHost)
				return &server;
		}
	}
	return defaultServer;
}

const ServerConfig* Router::selectServer(
	const HttpRequest& request,
	const RequestContext& context,
	int& errorStatusCode)
{
	std::string hostHeader;

	if (!HttpHeaderUtils::getValue(request.headers, "Host", hostHeader))
	{
		errorStatusCode = 400;
		if (!context.serverCandidates || context.serverCandidates->empty())
			return NULL;
		return &(*context.serverCandidates)[0];
	}
	return selectServerFromHost(hostHeader, context, errorStatusCode);
}

const LocationConfig* Router::selectLocation(
	const HttpRequest& request,
	const ServerConfig& server)
{
	const LocationConfig* best = NULL;
	std::size_t bestLength = 0;
	const std::vector<LocationConfig>& locations = server.getLocations();

	for (std::size_t i = 0; i < locations.size(); i++)
	{
		const std::string locationPath = normalizedLocationPath(locations[i].path);

		if (locationMatches(request.path, locationPath)
			&& locationPath.size() > bestLength)
		{
			best = &locations[i];
			bestLength = locationPath.size();
		}
	}
	return best;
}

RouteResult Router::route(
	const HttpRequest& request,
	const RequestContext& context)
{
	RouteResult result;

	result.server = selectServer(request, context, result.errorStatusCode);
	if (!result.server || result.errorStatusCode != 0)
		return result;

	// No matching location intentionally falls back to server-level settings.
	result.location = selectLocation(request, *result.server);
	return result;
}
