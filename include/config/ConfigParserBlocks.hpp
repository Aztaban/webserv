#ifndef CONFIGPARSERBLOCKS_HPP
# define CONFIGPARSERBLOCKS_HPP

# include "config/ServerConfig.hpp"
# include "config/ConfigParserUtils.hpp"
# include <set>
# include <stdexcept>
# include <string>
# include <vector>

namespace ConfigParserBlocks {

struct ParseState
{
	bool inServer;
	bool inLocation;
	std::set<std::string> serverSingleDirectives;
	std::set<std::string> locationSingleDirectives;

	ParseState()
		: inServer(false),
		inLocation(false),
		serverSingleDirectives(),
		locationSingleDirectives()
	{
	}
};

// Parses one cleaned config line and updates the current block state.
void parseConfigLine(
	const std::string& line,
	ParseState& state,
	ServerConfig& currentServer,
	LocationConfig& currentLocation,
	std::vector<ServerConfig>& servers);

}

#endif