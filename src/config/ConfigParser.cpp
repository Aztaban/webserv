#include "config/ConfigParser.hpp"

#include <sstream>

// Stores the config file path that will be parsed later.
ConfigParser::ConfigParser(const std::string& path)
	: _path(path)
{
}

static std::runtime_error makeLineError(
	const std::string& path,
	std::size_t lineNumber,
	const std::string& message)
{
	std::ostringstream stream;

	stream << path << ":" << lineNumber << ": " << message;
	return std::runtime_error(stream.str());
}

// Reads the config file, builds server blocks, and fills the config objects.
std::vector<ServerConfig> ConfigParser::parse() const
{
	std::ifstream input(_path.c_str());
	std::string rawLine;
	std::vector<ServerConfig> servers;
	ServerConfig currentServer;
	LocationConfig currentLocation;
	ConfigParserBlocks::ParseState state;
	std::size_t lineNumber = 0;

	if (!input.is_open())
		throw std::runtime_error("cannot open config file: " + _path);
	while (std::getline(input, rawLine))
	{
		lineNumber++;
		std::string line = ConfigParserUtils::trim(rawLine);
		std::size_t comment = line.find('#');

		if (comment != std::string::npos)
			line = ConfigParserUtils::trim(line.substr(0, comment));
		if (line.empty())
			continue;
		try
		{
			ConfigParserBlocks::parseConfigLine(line, state, currentServer, currentLocation, servers);
		}
		catch(const std::exception& e)
		{
			throw makeLineError(_path, lineNumber, e.what());
		}
		
		
	}
	if (state.inServer || state.inLocation)
		throw makeLineError(_path, lineNumber, "unclosed server or location block");
	if (servers.empty())
		throw std::runtime_error("no server blocks found in config file: " + _path);
	return servers;
}
