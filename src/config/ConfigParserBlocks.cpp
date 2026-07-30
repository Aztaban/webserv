#include "config/ConfigParserBlocks.hpp"

#include <algorithm>
#include <cctype>
#include <limits>

namespace
{

	using ConfigParserUtils::parseUnsignedDecimal;
	using ConfigParserUtils::splitTokens;
	using ConfigParserUtils::trim;

	// Return true when the line starts with a standalone configuration keyword.
	static bool startsWithKeyword(const std::string& line, const std::string& keyword)
	{
		if (line.compare(0, keyword.size(), keyword) != 0)
			return false;
		return line.size() == keyword.size() || std::isspace(static_cast<unsigned char>(line[keyword.size()]));
	}

	static std::string lowercase(const std::string& value)
	{
		std::string result = value;

		for (std::size_t i = 0; i < result.size(); i++)
			result[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(result[i])));
		return result;
	}

	static void markSingleDirective(std::set<std::string>& seen, const std::string& directive)
	{
		if (!seen.insert(directive).second)
			throw std::runtime_error("duplicate " + directive + " directive");
	}

	static bool hasServerName(
		const std::vector<std::string>& names,
		const std::string& candidate)
	{
		for (std::size_t i = 0; i < names.size(); ++i)
		{
			if (lowercase(names[i]) == lowercase(candidate))
				return true;
		}
		return false;
	}

	// Converts a listen value into a host/port pair and writes it into the server config.
	static void parseListenDirective(ServerConfig &server, const std::vector<std::string> &tokens)
	{
		std::string	value;
		std::string	host;
		std::string	portValue;
		std::size_t	colon;

		if (tokens.size() != 2)
			throw std::runtime_error("invalid listen directive");

		value = tokens[1];
		host = "127.0.0.1";
		colon = value.find(':');
		
		if (colon == std::string::npos)
			portValue = value;
		else
		{
			if (value.find(':', colon + 1) != std::string::npos)
				throw std::runtime_error("invalid listen directive");
			host = value.substr(0, colon);
			portValue = value.substr(colon + 1);
			if (host.empty())
				throw std::runtime_error("invalid listen host");
		}

		std::size_t port = parseUnsignedDecimal(portValue, 65535, "listen port");
		if (port == 0)
			throw std::runtime_error("listen port must be between 1 and 65535");
		server.setListen(host, static_cast<int>(port));
	}

	// Applies one server-level directive to the current ServerConfig.
	static void parseServerDirective(
		ServerConfig &server,
		std::set<std::string>& seenSingleDirectives,
		const std::vector<std::string> &tokens)
	{
		if (tokens.empty())
			throw std::runtime_error("empty directive");
		if (tokens[0] == "listen")
		{
			markSingleDirective(seenSingleDirectives, tokens[0]);
			parseListenDirective(server, tokens);
		}
		else if (tokens[0] == "root")
		{
			markSingleDirective(seenSingleDirectives, tokens[0]);
			if (tokens.size() != 2)
				throw std::runtime_error("invalid root directive");
			server.setRoot(tokens[1]);
		}
		else if (tokens[0] == "index")
		{
			markSingleDirective(seenSingleDirectives, tokens[0]);
			if (tokens.size() != 2)
				throw std::runtime_error("invalid index directive");
			server.setIndex(tokens[1]);
		}
		else if (tokens[0] == "client_max_body_size")
		{
			markSingleDirective(seenSingleDirectives, tokens[0]);
			if (tokens.size() != 2)
				throw std::runtime_error("invalid client_max_body_size directive");
			server.setClientMaxBodySize(parseUnsignedDecimal(tokens[1], std::numeric_limits<std::size_t>::max(), "client_max_body_size"));
		}
		else if (tokens[0] == "error_page")
		{
			if (tokens.size() != 3)
				throw std::runtime_error("invalid error_page directive");
			std::size_t status = parseUnsignedDecimal(tokens[1], 599, "error_page status");
			if (status < 400)
				throw std::runtime_error("error_page status must be between 400 and 599");
			if (server.getErrorpages().count(static_cast<int>(status)) != 0)
				throw std::runtime_error("duplicate error_page status: " + tokens[1]);
			server.addErrorPage(static_cast<int>(status), tokens[2]);
		}
		else if (tokens[0] == "server_name")
		{
			if (tokens.size() < 2)
				throw std::runtime_error("invalid server_name directive");
			for (std::size_t i = 1; i < tokens.size(); i++)
			{
				if (hasServerName(server.getServerNames(), tokens[i]))
					throw std::runtime_error("duplicate server_name value: " + tokens[i]);
				server.addServerName(tokens[i]);
			}
		}
		else
			throw std::runtime_error("unknown server directive: " + tokens[0]);
	}

	// Updates location root, index, autoindex, upload and redirect settings.
	static void parseLocationPropertyDirective(
		LocationConfig &location,
		std::set<std::string>& seenSingleDirectives,
		const std::vector<std::string> &tokens)
	{
		if (tokens[0] == "root")
		{
			markSingleDirective(seenSingleDirectives, tokens[0]);
			if (tokens.size() != 2)
				throw std::runtime_error("invalid root directive");
			location.root = tokens[1];
		}
		else if (tokens[0] == "index")
		{
			markSingleDirective(seenSingleDirectives, tokens[0]);
			if (tokens.size() != 2)
				throw std::runtime_error("invalid index directive");
			location.index = tokens[1];
		}
		else if (tokens[0] == "autoindex")
		{
			markSingleDirective(seenSingleDirectives, tokens[0]);
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
			markSingleDirective(seenSingleDirectives, tokens[0]);
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
			markSingleDirective(seenSingleDirectives, tokens[0]);
			if (tokens.size() != 2)
				throw std::runtime_error("invalid upload_path directive");
			location.uploadPath = tokens[1];
		}
		else if (tokens[0] == "return")
		{
			markSingleDirective(seenSingleDirectives, tokens[0]);
			if (tokens.size() != 3)
				throw std::runtime_error("invalid return directive");
			std::size_t status = parseUnsignedDecimal(tokens[1], 302, "redirect status");
			if (status != 301 && status != 302)
				throw std::runtime_error("redirect status must be 301 or 302");
			location.hasRedirect = true;
			location.redirectCode = static_cast<int>(status);
			location.redirectUrl = tokens[2];
		}
		else if (tokens[0] == "server_name")
			throw std::runtime_error("server_name is not allowed in a location block");
		else
			throw std::runtime_error("unknown location directive: " + tokens[0]);
	}

	// Updates location methods and CGI interpreter mappings.
	static void parseLocationCollectionDirective(
		LocationConfig &location,
		std::set<std::string>& seenSingleDirectives,
		const std::vector<std::string> &tokens)
	{
		if (tokens[0] == "methods")
		{
			markSingleDirective(seenSingleDirectives, tokens[0]);

			if (tokens.size() < 2)
				throw std::runtime_error("methods directive requires at least one method");
			location.methods.clear();
			for (std::size_t i = 1; i < tokens.size(); i++)
			{
				if (tokens[i] != "GET" && tokens[i] != "POST" && tokens[i] != "DELETE")
					throw std::runtime_error("invalid method in methods directive: " + tokens[i]);
				if (std::find(location.methods.begin(), location.methods.end(), tokens[i]) != location.methods.end())
					throw std::runtime_error("duplicate method in methods directive: " + tokens[i]);
				location.methods.push_back(tokens[i]);
			}
		}
		else if (tokens[0] == "cgi_extension")
		{
			if (tokens.size() < 3 || (tokens.size() - 1) % 2 != 0)
				throw std::runtime_error("cgi_extension requires extension/interpreter pairs");
			for (std::size_t i = 1; i + 1 < tokens.size(); i += 2)
			{
				if (tokens[i].size() < 2 || tokens[i][0] != '.')
					throw std::runtime_error("invalid CGI extension: " + tokens[i]);
				if (tokens[i + 1].empty())
					throw std::runtime_error("empty CGI interpreter for extension: " + tokens[i]);
				if (location.cgiExtensions.count(tokens[i]) != 0)
					throw std::runtime_error("duplicate CGI extension mapping: " + tokens[i]);
				location.cgiExtensions[tokens[i]] = tokens[i + 1];
			}
			location.cgiEnabled = true;
		}
	}

	// Applies one location-level directive to the current LocationConfig.
	static void parseLocationDirective(
		LocationConfig &location,
		std::set<std::string>& seenSingleDirectives,
		const std::vector<std::string> &tokens)
	{
		if (tokens.empty())
			throw std::runtime_error("empty directive");
		if (tokens[0] == "methods" || tokens[0] == "cgi_extension")
			parseLocationCollectionDirective(location, seenSingleDirectives, tokens);
		else
			parseLocationPropertyDirective(location, seenSingleDirectives, tokens);
	}

	static void validateLocationSettings(const LocationConfig& location)
	{
		if (location.uploadEnabled && location.uploadPath.empty())
			throw std::runtime_error("upload directive requires an upload_path in location " + location.path);
	}

	static void validateUniqueLocationPath(const ServerConfig& server, const LocationConfig& location)
	{
		const std::vector<LocationConfig>& locations = server.getLocations();

		for (std::size_t i = 0; i < locations.size(); i++)
		{
			if (locations[i].path == location.path)
				throw std::runtime_error("duplicate location path: " + location.path);
		}
	}

	// Starts a new server block and resets the working server config.
	static void beginServerBlock(ConfigParserBlocks::ParseState& state, ServerConfig &currentServer)
	{
		if (state.inServer)
			throw std::runtime_error("nested server blocks are not allowed");
		currentServer = ServerConfig();
		state.inServer = true;
		state.serverSingleDirectives.clear();
		state.locationSingleDirectives.clear();
	}

	// Closes the current block and either stores a location or a server.
	static void closeBlock(
			ConfigParserBlocks::ParseState& state,
			ServerConfig &currentServer,
			LocationConfig &currentLocation,
			std::vector<ServerConfig> &servers)
	{
		if (state.inLocation)
		{
			validateLocationSettings(currentLocation);
			validateUniqueLocationPath(currentServer, currentLocation);
			currentServer.addLocation(currentLocation);
			currentLocation = LocationConfig();
			state.inLocation = false;
			state.locationSingleDirectives.clear();
			return;
		}
		if (state.inServer)
		{
			servers.push_back(currentServer);
			currentServer = ServerConfig();
			state.inServer = false;
			state.serverSingleDirectives.clear();
			return;
		}
		throw std::runtime_error("unexpected closing brace");
	}

// Validates and normalizes a location URI prefix
static std::string	normalizeLocationPath(const std::string& value)
{
	std::string	path = value;

	if (path.empty() || path[0] != '/')
		throw std::runtime_error("location path must start with '/'");
	if (path.find('\\') != std::string::npos
			|| path.find('?') != std::string::npos
			|| path.find('#') != std::string::npos
			|| path.find('{') != std::string::npos
			|| path.find('}') != std::string::npos)
			throw std::runtime_error("malformed location path");
	for (std::size_t i = 0; i < path.size(); i++)
	{
		if (std::isspace(static_cast<unsigned char>(path[i])))
			throw std::runtime_error("malformed location path");
	}
	while (path.size() > 1 && path[path.size() - 1] == '/')
		path.erase(path.size() - 1);
	if (path.find("//") != std::string::npos)
		throw std::runtime_error("malformed location path");

	std::size_t	segmentBegin = 1;
	while (segmentBegin <= path.size())
	{
		std::size_t	segmentEnd = path.find('/', segmentBegin);
		std::string	segment;

		if (segmentEnd == std::string::npos)
			segment = path.substr(segmentBegin);
		else
			segment = path.substr(segmentBegin, segmentEnd - segmentBegin);
		if (segment == "." || segment == "..")
			throw std::runtime_error("malformed location path");
		if (segmentEnd == std::string::npos)
			break;
		segmentBegin = segmentEnd + 1;
	}
	return path;
}

	// Opens a new location block and stores its normalized path.
	static void beginLocationBlock(
			const std::string &line,
			ConfigParserBlocks::ParseState& state,
			LocationConfig &currentLocation)
	{
		std::size_t brace = line.find('{');
		std::size_t pathBegin = std::string("location ").size();
		std::string locationPath;

		if (state.inLocation)
			throw std::runtime_error("nested location blocks are not allowed");
		if (brace == std::string::npos)
			throw std::runtime_error("location block must open with '{'");
		if (brace == 0 || !std::isspace(static_cast<unsigned char>(line[brace - 1])))
			throw std::runtime_error("location path must be separated from '{'");
		if (!trim(line.substr(brace + 1)).empty())
			throw std::runtime_error("unexpected text after '{'");
		locationPath = trim(line.substr(pathBegin, brace - pathBegin));
		if (locationPath.empty())
			throw std::runtime_error("empty location path");
		currentLocation = LocationConfig();
		currentLocation.path = normalizeLocationPath(locationPath);
		state.inLocation = true;
		state.locationSingleDirectives.clear();
	}

	// Sends one directive line to the right parser depending on the current block.
	static void dispatchDirectiveLine(
			const std::string &line,
			ConfigParserBlocks::ParseState& state,
			ServerConfig &currentServer,
			LocationConfig &currentLocation)
	{
		std::string directive;
		std::vector<std::string> tokens;

		if (line.empty() || line[line.size() - 1] != ';')
			throw std::runtime_error("directive must end with ';'");
		directive = trim(line.substr(0, line.size() - 1));
		if (directive.empty())
			throw std::runtime_error("empty directive");
		if (directive.find(';') != std::string::npos)
			throw std::runtime_error("multiple directives on one line are not allowed");
		if (directive.find('{') != std::string::npos
			|| directive.find('}') != std::string::npos)
			throw std::runtime_error("unexpected brace in directive");
		tokens = splitTokens(directive);
		if (state.inLocation)
			parseLocationDirective(currentLocation, state.locationSingleDirectives, tokens);
		else
			parseServerDirective(currentServer, state.locationSingleDirectives, tokens);
	}

} // namespace

// Parses a single cleaned config line and updates the current block state.
void ConfigParserBlocks::parseConfigLine(
		const std::string &line,
		ParseState& state,
		ServerConfig &currentServer,
		LocationConfig &currentLocation,
		std::vector<ServerConfig> &servers)
{
	if (line == "server {")
	{
		beginServerBlock(state, currentServer);
		return;
	}
	if (startsWithKeyword(line, "server"))
		throw std::runtime_error("invalid server block declaration");
	if (!line.empty() && line[0] == '}')
	{
		if (line != "}")
			throw std::runtime_error("unexpected text after '}'");
		closeBlock(state, currentServer, currentLocation, servers);
		return;
	}
	if (startsWithKeyword(line, "location"))
	{
		if (!state.inServer)
			throw std::runtime_error("location block outside server block");
		beginLocationBlock(line, state, currentLocation);
		return;
	}
	if (!state.inServer)
		throw std::runtime_error("directive outside server block: " + line);

	dispatchDirectiveLine(line, state, currentServer, currentLocation);
}
