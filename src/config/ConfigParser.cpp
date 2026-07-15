#include "config/ConfigParser.hpp"

// Stores the config file path that will be parsed later.
ConfigParser::ConfigParser(const std::string& path)
	: _path(path)
{
}

// Reads the config file, builds server blocks, and fills the config objects.
std::vector<ServerConfig> ConfigParser::parse() const
{
	std::ifstream input(_path.c_str());
	std::string rawLine;
	std::vector<ServerConfig> servers;
	ServerConfig currentServer;
	LocationConfig currentLocation;
	bool inServer = false;
	bool inLocation = false;

	if (!input.is_open())
		throw std::runtime_error("cannot open config file: " + _path);
	while (std::getline(input, rawLine))
	{
		std::string line = ConfigParserUtils::trim(rawLine);
		std::size_t comment = line.find('#');

		if (comment != std::string::npos)
			line = ConfigParserUtils::trim(line.substr(0, comment));
		if (line.empty())
			continue;
		ConfigParserBlocks::parseConfigLine(line, inServer, inLocation, currentServer, currentLocation, servers);
	}
	if (inServer || inLocation)
		throw std::runtime_error("unclosed server or location block");
	if (servers.empty())
		throw std::runtime_error("no server blocks found in config file");
	return servers;
}
