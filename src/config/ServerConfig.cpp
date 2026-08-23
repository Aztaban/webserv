/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ServerConfig.cpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: lformank <lformank@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/08/19 11:13:39 by lformank          #+#    #+#             */
/*   Updated: 2026/08/19 11:13:39 by lformank         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "config/ServerConfig.hpp"

ServerConfig::ServerConfig()
    : _serverName("default"),
	  _host("127.0.0.1"),
      _port(8000),
      _root("./www/site1"),
      _index("index.html"),
      _clientMaxBodySize(1000000),
      _errorPages(),
      _locations()
{
}

ServerConfig::ServerConfig(const std::string& name, const std::string& host, int port)
	: _serverName(name),
	  _host(host),
	  _port(port),
	  _root("./www/site1"),
	  _index("index.html"),
	  _clientMaxBodySize(1000000),
	  _errorPages(),
	  _locations()
{
}

const std::string&	ServerConfig::getName() const {
    return _serverName;
}

const   std::string& ServerConfig::getHost() const {
    return _host;
}

int ServerConfig::getPort() const {
    return _port;
}

const   std::string&  ServerConfig::getRoot() const {
    return _root;
}

std::size_t ServerConfig::getClientMaxBodySize() const
{
    return _clientMaxBodySize;
}

const std::string&  ServerConfig::getIndex() const
{
	return _index;
}

const std::map<int, std::string>&   ServerConfig::getErrorpages() const
{
	return _errorPages;
}

const std::vector<LocationConfig>&  ServerConfig::getLocations() const
{
	return _locations;
}

void    ServerConfig::setName(const std::string& name)
{
    _serverName = name;
}

void	ServerConfig::setHost(const std::string& host)
{
	this->_host = host;
}

void	ServerConfig::setPort(int port)
{
	this->_port = port;
}

void    ServerConfig::setRoot(const std::string& root) {
    _root = root;
}

void    ServerConfig::setIndex(const std::string& index)
{
	_index = index;
}

void    ServerConfig::setClientMaxBodySize(std::size_t size)
{
    _clientMaxBodySize = size;
}

void    ServerConfig::addErrorPage(int statusCode, const std::string& path)
{
	_errorPages[statusCode] = path;
}

void    ServerConfig::addLocation(const LocationConfig& location)
{
	_locations.push_back(location);
}

