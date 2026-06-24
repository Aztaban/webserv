#include "config/ServerConfig.hpp"

ServerConfig::ServerConfig() : _host("127.0.0.1"), _port(8000) {}
ServerConfig::ServerConfig(const std::string &host, int port) : _host(host), _port(port) {}

const   std::string& ServerConfig::getHost() const {
    return _host;
}

int ServerConfig::getPort() const {
    return _port;
}

const   std::string&  ServerConfig::getRoot() const {
    return _root;
}

void    ServerConfig::setRoot(const std::string& root) {
    _root = root;
}
