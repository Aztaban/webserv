/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Router.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: lformank <lformank@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/08/27 15:18:09 by lformank          #+#    #+#             */
/*   Updated: 2026/09/28 13:20:32 by lformank         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "routing/Router.hpp"
#include "http/HttpHeaderUtils.hpp"

#include <algorithm>
#include <cctype>

Router::Router() {}
Router::~Router() {}

static std::string toLowerCopy(const std::string& value)
{
	std::string lower = value;
	for (std::size_t i = 0; i < lower.size(); ++i)
		lower[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(lower[i])));
	return lower;
}

static std::string trimWhitespace(const std::string& value)
{
	std::size_t start = 0;
	while (start < value.size() && (value[start] == ' ' || value[start] == '\t'))
		++start;
	std::size_t end = value.size();
	while (end > start && (value[end - 1] == ' ' || value[end - 1] == '\t'))
		--end;
	return value.substr(start, end - start);
}

static std::string normalizeHost(const std::string& value)
{
	std::string host = trimWhitespace(value);
	if (host.empty())
		return "";

	std::size_t colon = host.rfind(':');
	if (colon != std::string::npos)
	{
		if (host.find(':', colon + 1) != std::string::npos)
			return "";
		std::string port = host.substr(colon + 1);
		if (port.empty() || port.find_first_not_of("0123456789") != std::string::npos)
			return "";
		host = host.substr(0, colon);
	}

	if (host.empty())
		return "";
	return host;
}

bool prefixCheck(const std::string& requestPath, const std::string& locationPath)
{
	if (locationPath.empty())
		return false;
	if (requestPath == locationPath)
		return true;
	if (requestPath.compare(0, locationPath.size(), locationPath) != 0)
		return false;
	if (locationPath[locationPath.size() - 1] == '/')
		return true;
	if (requestPath.size() == locationPath.size())
		return false;
	return requestPath[locationPath.size()] == '/';
}

const ServerConfig* Router::matchServer(const std::vector<ServerConfig>& servers, const HttpRequest& request) const
{
	if (servers.empty())
		return NULL;

	std::string hostValue;
	if (!HttpHeaderUtils::getValue(request.headers, "host", hostValue))
		return &servers[0];
	
	std::string host = normalizeHost(hostValue);
	if (host.empty())
		return NULL;

	std::string normalizedHost = toLowerCopy(host);
	for (std::size_t i = 0; i < servers.size(); ++i)
	{
		const std::vector<std::string>& serverNames = servers[i].getServerNames();
		for (std::size_t j = 0; j < serverNames.size(); ++j)
		{
			std::string serverName = toLowerCopy(serverNames[j]);
			if (serverName == normalizedHost)
				return &servers[i];
		}
	}
	return &servers[0];
}

const LocationConfig* Router::matchLocation(const HttpRequest& request, const ServerConfig& server) const
{
	const std::vector<LocationConfig>& locations = server.getLocations();
	const LocationConfig* longestLocation = NULL;
	for(size_t i = 0; i < locations.size(); i++)
	{
		if (!prefixCheck(request.path, locations[i].path))
			continue;
		if (longestLocation == NULL || locations[i].path.size() > longestLocation->path.size())
			longestLocation = &locations[i];
	}
	return longestLocation;
}


// location = router.matchLocation(request, server)
// if location == NULL       -> 404
// if method not in location->methods -> 405
// else                       -> dispatch as before, using location's root/etc.