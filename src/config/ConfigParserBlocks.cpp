#include "config/ConfigParserBlocks.hpp"

namespace {

using ConfigParserUtils::isNumber;
using ConfigParserUtils::splitTokens;
using ConfigParserUtils::trim;

// Converts a listen value into a host/port pair and writes it into the server config.
static void parseListenDirective(ServerConfig& server, const std::vector<std::string>& tokens)
{
	std::string value;
	std::string host;
	std::size_t colon;

	if (tokens.size() != 2)
		throw std::runtime_error("invalid listen directive");

	value = tokens[1];
	host = "127.0.0.1";
	colon = value.find(':');
	if (colon == std::string::npos)
	{
		if (!isNumber(value))
			throw std::runtime_error("invalid listen port");
		server = ServerConfig(host, std::atoi(value.c_str()));
		return;
	}
	if (!isNumber(value.substr(colon + 1)))
		throw std::runtime_error("invalid listen port");
	host = value.substr(0, colon);
	server = ServerConfig(host, std::atoi(value.substr(colon + 1).c_str()));
}

// Applies one server-level directive to the current ServerConfig.
static void parseServerDirective(ServerConfig& server, const std::vector<std::string>& tokens)
{
	if (tokens.empty())
		return;
	if (tokens[0] == "listen")
		parseListenDirective(server, tokens);
	else if (tokens[0] == "root")
	{
		if (tokens.size() != 2)
			throw std::runtime_error("invalid root directive");
		server.setRoot(tokens[1]);
	}
	else if (tokens[0] == "index")
	{
		if (tokens.size() != 2)
			throw std::runtime_error("invalid index directive");
		server.setIndex(tokens[1]);
	}
	else if (tokens[0] == "client_max_body_size")
	{
		if (tokens.size() != 2 || !isNumber(tokens[1]))
			throw std::runtime_error("invalid client_max_body_size directive");
		server.setClientMaxBodySize(static_cast<std::size_t>(std::atoi(tokens[1].c_str())));
	}
	else if (tokens[0] == "error_page")
	{
		if (tokens.size() != 3 || !isNumber(tokens[1]))
			throw std::runtime_error("invalid error_page directive");
		server.addErrorPage(std::atoi(tokens[1].c_str()), tokens[2]);
	}
	else if (tokens[0] == "server_name")
		return;
	else
		throw std::runtime_error("unknown server directive: " + tokens[0]);
}

// Updates location root, index, autoindex, upload and redirect settings.
static void parseLocationPropertyDirective(LocationConfig& location, const std::vector<std::string>& tokens)
{
	if (tokens[0] == "root")
	{
		if (tokens.size() != 2)
			throw std::runtime_error("invalid root directive");
		location.root = tokens[1];
	}
	else if (tokens[0] == "index")
	{
		if (tokens.size() != 2)
			throw std::runtime_error("invalid index directive");
		location.index = tokens[1];
	}
	else if (tokens[0] == "autoindex")
	{
		if (tokens.size() != 2)
			throw std::runtime_error("invalid autoindex directive");
		if (tokens[1] == "on")
			location.autoindex = true;
		else if (tokens[1] == "off")
			location.autoindex = false;
		else
			throw std::runtime_error("invalid autoindex value");
	}
	else if (tokens[0] == "upload")
	{
		if (tokens.size() != 2)
			throw std::runtime_error("invalid upload directive");
		if (tokens[1] == "on")
			location.uploadEnabled = true;
		else if (tokens[1] == "off")
			location.uploadEnabled = false;
		else
			throw std::runtime_error("invalid upload value");
	}
	else if (tokens[0] == "upload_path")
	{
		if (tokens.size() != 2)
			throw std::runtime_error("invalid upload_path directive");
		location.uploadPath = tokens[1];
	}
	else if (tokens[0] == "return")
	{
		if (tokens.size() != 3 || !isNumber(tokens[1]))
			throw std::runtime_error("invalid return directive");
		location.hasRedirect = true;
		location.redirectCode = std::atoi(tokens[1].c_str());
		location.redirectUrl = tokens[2];
	}
	else if (tokens[0] == "server_name")
		return;
	else
		throw std::runtime_error("unknown location directive: " + tokens[0]);
}

// Updates location methods and CGI interpreter mappings.
static void parseLocationCollectionDirective(LocationConfig& location, const std::vector<std::string>& tokens)
{
	if (tokens[0] == "methods")
	{
		std::size_t i = 1;

		location.methods.clear();
		while (i < tokens.size())
		{
			if (tokens[i] != "GET" && tokens[i] != "POST" && tokens[i] != "DELETE")
				throw std::runtime_error("invalid method in methods directive");
			location.methods.push_back(tokens[i]);
			i++;
		}
	}
	else if (tokens[0] == "cgi_extension")
	{
		std::size_t i = 1;

		if ((tokens.size() - 1) % 2 != 0)
			throw std::runtime_error("invalid cgi_extension directive");
		while (i + 1 < tokens.size())
		{
			location.cgiExtensions[tokens[i]] = tokens[i + 1];
			i += 2;
		}
		location.cgiEnabled = !location.cgiExtensions.empty();
	}
}

// Applies one location-level directive to the current LocationConfig.
static void parseLocationDirective(LocationConfig& location, const std::vector<std::string>& tokens)
{
	if (tokens.empty())
		return;
	if (tokens[0] == "methods" || tokens[0] == "cgi_extension")
		parseLocationCollectionDirective(location, tokens);
	else
		parseLocationPropertyDirective(location, tokens);
}

// Starts a new server block and resets the working server config.
static void beginServerBlock(bool& inServer, ServerConfig& currentServer)
{
	if (inServer)
		throw std::runtime_error("nested server blocks are not allowed");
	currentServer = ServerConfig();
	inServer = true;
}

// Closes the current block and either stores a location or a server.
static void closeBlock(
	bool& inServer,
	bool& inLocation,
	ServerConfig& currentServer,
	LocationConfig& currentLocation,
	std::vector<ServerConfig>& servers)
{
	if (inLocation)
	{
		currentServer.addLocation(currentLocation);
		currentLocation = LocationConfig();
		inLocation = false;
		return;
	}
	if (inServer)
	{
		servers.push_back(currentServer);
		currentServer = ServerConfig();
		inServer = false;
		return;
	}
	throw std::runtime_error("unexpected closing brace");
}

// Opens a new location block and stores its path.
static void beginLocationBlock(
	const std::string& line,
	bool& inLocation,
	LocationConfig& currentLocation)
{
	std::size_t brace = line.find('{');
	std::size_t pathBegin = std::string("location ").size();
	std::string locationPath;

	if (inLocation)
		throw std::runtime_error("nested location blocks are not allowed");
	if (brace == std::string::npos)
		throw std::runtime_error("location block must open with '{'");
	locationPath = trim(line.substr(pathBegin, brace - pathBegin));
	if (locationPath.empty())
		throw std::runtime_error("empty location path");
	currentLocation = LocationConfig();
	currentLocation.path = locationPath;
	inLocation = true;
}

// Sends one directive line to the right parser depending on the current block.
static void dispatchDirectiveLine(
	const std::string& line,
	bool inLocation,
	ServerConfig& currentServer,
	LocationConfig& currentLocation)
{
	std::string directive = line;

	if (!directive.empty() && directive[directive.size() - 1] == ';')
		directive = trim(directive.substr(0, directive.size() - 1));
	if (inLocation)
		parseLocationDirective(currentLocation, splitTokens(directive));
	else
		parseServerDirective(currentServer, splitTokens(directive));
}

} // namespace

// Parses a single cleaned config line and updates the current block state.
void ConfigParserBlocks::parseConfigLine(
	const std::string& line,
	bool& inServer,
	bool& inLocation,
	ServerConfig& currentServer,
	LocationConfig& currentLocation,
	std::vector<ServerConfig>& servers)
{
	if (line == "server {")
	{
		beginServerBlock(inServer, currentServer);
		return;
	}
	if (line == "}")
	{
		closeBlock(inServer, inLocation, currentServer, currentLocation, servers);
		return;
	}
	if (!inServer)
		throw std::runtime_error("directive outside server block: " + line);

	if (line.rfind("location ", 0) == 0)
	{
		beginLocationBlock(line, inLocation, currentLocation);
		return;
	}

	dispatchDirectiveLine(line, inLocation, currentServer, currentLocation);
}
