#include "config/ServerConfig.hpp"

ServerConfig::ServerConfig() : _host("127.0.0.1"), _port(8000) {}
ServerConfig::ServerConfig(const std::string &host, int port) : _host(host), _port(port) {}

const std::string& ServerConfig::getHost() const {
    return _host;
}

int ServerConfig::getPort() const {
    return _port;
}