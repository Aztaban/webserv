#ifndef CONFIGPARSER_HPP
# define CONFIGPARSER_HPP

#include "config/ServerConfig.hpp"
#include "config/ConfigParserUtils.hpp"
#include "config/ConfigParserBlocks.hpp"

#include <cstdlib>
#include <fstream>
#include <stdexcept>
# include <string>
# include <vector>

class ConfigParser {

private:
	std::string	_path;

public:
	// Stores the config file path that will be parsed later.
	ConfigParser(const std::string& path);

	// Reads the config file and returns the parsed server blocks.
	std::vector<ServerConfig> parse() const;
};

#endif