#include "config/ServerConfig.hpp"

// Sets the default server values used when the config file does not override them.
ServerConfig::ServerConfig()
	: _host("127.0.0.1"),
	_port(8000),
	_root("./www/site1"),
	_index("index.html"),
	_clientMaxBodySize(1000000),
	_errorPages(),
	_locations()
{}

// Builds a server config for a specific listener host and port.
ServerConfig::ServerConfig(const std::string& host, int port)
	: _host(host),
	_port(port),
	_root("./www/site1"),
	_index("index.html"),
	_clientMaxBodySize(1000000),
	_errorPages(),
	_locations()
{
}

// Returns the host this server listens on.
const std::string& ServerConfig::getHost() const {
	return _host;
}

// Returns the port this server listens on.
int ServerConfig::getPort() const {
	return _port;
}

// Returns the server root used as the base filesystem path.
const std::string&  ServerConfig::getRoot() const {
	return _root;
}

// Returns the default index file name for directories.
const std::string& ServerConfig::getIndex() const
{
	return _index;
}

// Returns the configured custom error pages by status code.
const std::map<int, std::string>& ServerConfig::getErrorpages() const
{
	return _errorPages;
}

// Returns all location blocks attached to this server.
const std::vector<LocationConfig>& ServerConfig::getLocations() const
{
	return _locations;
}

// Updates the default index file name.
void ServerConfig::setIndex(const std::string& index) {
	_index = index;
}

// Updates the filesystem root used by this server.
void ServerConfig::setRoot(const std::string& root) {
	_root = root;
}

// Returns the maximum allowed request body size for this server.
std::size_t ServerConfig::getClientMaxBodySize() const
{
	return _clientMaxBodySize;
}

// Updates the request body size limit for this server.
void ServerConfig::setClientMaxBodySize(std::size_t size)
{
	_clientMaxBodySize = size;
}

// Registers a custom file to serve for one HTTP status code.
void ServerConfig::addErrorPage(int statusCode, const std::string& path)
{
	_errorPages[statusCode] = path;
}

// Adds one location block to the server configuration.
void ServerConfig::addLocation(const LocationConfig& location)
{
	_locations.push_back(location);
}
