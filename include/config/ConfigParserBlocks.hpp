#ifndef CONFIGPARSERBLOCKS_HPP
# define CONFIGPARSERBLOCKS_HPP

# include "config/ServerConfig.hpp"
#include "config/ConfigParserUtils.hpp"
#include <cstdlib>
#include <stdexcept>
# include <string>
# include <vector>

namespace ConfigParserBlocks {

// Parses one cleaned config line and updates the current block state.
void parseConfigLine(
	const std::string& line,
	bool& inServer,
	bool& inLocation,
	ServerConfig& currentServer,
	LocationConfig& currentLocation,
	std::vector<ServerConfig>& servers);

}

#endif