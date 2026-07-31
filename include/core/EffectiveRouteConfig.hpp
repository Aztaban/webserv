#ifndef EFFECTIVEROUTECONFIG_HPP
# define EFFECTIVEROUTECONFIG_HPP

# include <cstddef>
# include <map>
# include <string>
# include <vector>

# include "config/LocationConfig.hpp"
# include "config/ServerConfig.hpp"

// Fully resolved request-handling configuration for one selected route.
//
// Router owns virtual-server and location selection. RequestHandler owns
// inheritance and produces this structure once. Redirect and CGI handlers
// should consume it instead of repeating routing or configuration merging.
struct EffectiveRouteConfig
{
  const ServerConfig*   server;
  const LocationConfig* location;

	std::string root;
	std::string locationPath;
  std::string uploadDirectory;
  std::string index;
  std::vector<std::string>  methods;
  std::map<int, std::string>  errorPages;
  bool  autoindex;
  bool  uploadEnabled;
  std::size_t maxBodySize;

  bool  hasRedirect;
  int   redirectCode;
  std::string redirectUrl;

  bool  cgiEnabled;
  std::map<std::string, std::string>  cgiExtensions;

  EffectiveRouteConfig()
    : server(NULL),
      location(NULL),
      root(),
      locationPath("/"),
      uploadDirectory(),
      index(),
      methods(),
      errorPages(),
      autoindex(false),
      uploadEnabled(false),
      maxBodySize(0),
      hasRedirect(false),
      redirectCode(0),
      redirectUrl(),
      cgiEnabled(false),
      cgiExtensions()
  {}
};

#endif