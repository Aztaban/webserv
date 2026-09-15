/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Router.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: lformank <lformank@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/08/27 15:18:09 by lformank          #+#    #+#             */
/*   Updated: 2026/08/27 16:57:49 by lformank         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "routing/Router.hpp"

Router::Router() {}
Router::~Router() {}

bool prefixCheck(const std::string& requestPath, const std::string& locationPath)
{
	size_t i = 0;
	while(i < locationPath.size() && requestPath[i] == locationPath[i])
		i++;
	if (i == locationPath.size() && (requestPath[i] == '/' || requestPath[i] == '\0'
			|| locationPath[locationPath.size() - 1] == '/'))
		return true;
	return false;
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