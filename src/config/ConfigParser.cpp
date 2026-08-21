/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ConfigParser.cpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: lformank <lformank@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/08/13 13:21:53 by lformank          #+#    #+#             */
/*   Updated: 2026/08/21 17:34:36 by lformank         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "config/ConfigParser.hpp"

#include <fstream>
#include <sstream>
#include <iostream>
#include <string>
#include <cstdlib>
#include "config/Token.hpp"
#include "config/TokenStream.hpp"

ConfigParser::ConfigParser() : _tokens(), _path(), _source()
{}

ConfigParser::ConfigParser(const std::string& path) : _tokens(), _path(path), _source()
{}

const ConfigParser& ConfigParser::operator=(const ConfigParser& other)
{
	if (this != &other)
	{
		this->_tokens.clear();
		this->_tokens.insert(_tokens.begin(), other._tokens.begin(), other._tokens.end());
		this->_path = other._path;
		this->_source = other._source;
	}
	return *this;
}

ConfigParser::ConfigParser(const ConfigParser& other)
{
	*this = other;
}

ConfigParser::~ConfigParser() {}

const std::string& ConfigParser::getSource()
{
	return _source;
}

void	ConfigParser::loadSource()
{
	std::ifstream file(_path.c_str());
	std::ostringstream oss;
	oss << file.rdbuf();
	_source = oss.str();
}

void ConfigParser::pushToken(const std::string& str, TokenType type)
{
	Token token;
	token.type = type;
	token.value = str;
	_tokens.push_back(token);
}

void ConfigParser::pushWord(std::string& word)
{
	if (!word.empty())
	{
		pushToken(word, T_WORD);
		word.clear();
	}
}

void ConfigParser::tokenize()
{
	std::string word;

	for(size_t i = 0; i < _source.size(); i++)
	{
		if (!isspace((unsigned char)_source[i]))
		{
			if (_source[i] == '{')
			{
				pushWord(word);
				pushToken("", T_LBRACE);
			}
			else if (_source[i] == '}')
			{
				pushWord(word);
				pushToken("", T_RBRACE);
			}
			else if (_source[i] == ';')
			{
				pushWord(word);
				pushToken("", T_SEMICOLON);
			}
			else if (_source[i] == '"')
			{
				pushWord(word);
				std::string str;
				while (_source[++i] && _source[i] != '"')
					str += _source[i];
				if (_source[i] == '\0')
					throw std::runtime_error("unterminated string config file: " + _path);
				pushToken(str, T_STRING);
			}
			else if (_source[i] == '#')
			{
				pushWord(word);
				while (_source[++i] && _source[i] != '\n') {}
				pushToken("", T_COMMENT);
			}
			else
				word += _source[i];
		}
		else
			pushWord(word);
	}
	pushWord(word);
}

bool validBool(TokenStream& tokenStream)
{
	Token token = tokenStream.expect(T_WORD);

	tokenStream.expect(T_SEMICOLON);
	if (token.value == "true")
		return true;
	else if (token.value != "false")
		throw std::runtime_error("Invalid bool value: " + token.value);
	return false;
}

std::string checkRedirectUrl(TokenStream& tokenStream)
{
	std::string str;

	if (tokenStream.peek().type == T_WORD || tokenStream.peek().type == T_STRING)
	{
		str = tokenStream.advance().value;
		tokenStream.expect(T_SEMICOLON);
		return str;
	}
	throw std::runtime_error("Missing redirectUrl value");
}

const std::vector<std::string>& checkMethods(TokenStream& tokenStream)
{
	Token token = tokenStream.advance();
	std::vector<std::string>* vector = new std::vector<std::string>;

	while (token.type != T_SEMICOLON || token.type != T_RBRACE)
	{
		if (token.value == "GET" || token.value == "POST" || token.value == "DELETE")
		{
			vector->push_back(token.value);
			token = tokenStream.advance();
		}
		else
			throw std::runtime_error("Invalid method: " + token.value);
	}
	tokenStream.expect(T_SEMICOLON);
	return *vector;
}

const std::map<std::string, std::string>& checkCgiExtensions(TokenStream& tokenStream)
{
	std::map<std::string, std::string>* map = new std::map<std::string, std::string>;
	Token token;
	std::string extension;
	std::string path;
	
	while (token.type != T_SEMICOLON || token.type != T_RBRACE)
	{
		if (token.value[0] == '.')
		{
			extension = token.value;
			token = tokenStream.advance();
		}
		if (token.value[0] == '/' || token.value.find("https://", 0) != std::string::npos  || token.value.find("http://", 0) != std::string::npos )
		{
			path = token.value;
			token = tokenStream.expect(T_SEMICOLON);
			token = tokenStream.advance();
		}
		else
			throw std::runtime_error("Invalid extension: " + token.value);
	}
	tokenStream.expect(T_SEMICOLON);
	return *map;
}

void parseLocation(TokenStream& tokenStream, ServerConfig& serverConfig)
{
	std::cout << "Entering parse location! " << tokenStream.peek().value << std::endl;
	Token token = tokenStream.expect(T_WORD);
	LocationConfig locationConfig;

	if (token.value[0] != '/')
		throw std::runtime_error("Invalid location's address: " + token.value);
	locationConfig.path = token.value;
	tokenStream.expect(T_LBRACE);
	token = tokenStream.expect(T_WORD);
	while (token.type != T_RBRACE)
	{
		if (token.value == "root" && (tokenStream.peek().type == T_WORD || tokenStream.peek().type == T_STRING))
		{
			locationConfig.root = tokenStream.advance().value;
			tokenStream.expect(T_SEMICOLON);
		}
		else if (token.value == "index" && (tokenStream.peek().type == T_WORD || tokenStream.peek().type == T_STRING))
		{
			locationConfig.index = tokenStream.advance().value;
			tokenStream.expect(T_SEMICOLON);
		}
		else if (token.value == "methods")
			locationConfig.methods = checkMethods(tokenStream);
		else if (token.value == "autoindex")
			locationConfig.autoindex = validBool(tokenStream);
		else if (token.value == "hasRedirect")
			locationConfig.hasRedirect = validBool(tokenStream);
		else if (token.value == "redirectCode")
			locationConfig.redirectCode = validBool(tokenStream);
		else if (token.value == "redirectUrl" && (tokenStream.peek().type == T_WORD || tokenStream.peek().type == T_STRING))
			locationConfig.redirectUrl = checkRedirectUrl(tokenStream);
		else if (token.value == "uploadEnabled" )
			locationConfig.uploadEnabled = validBool(tokenStream);
		else if (token.value == "uploadPath" && (tokenStream.peek().type == T_WORD || tokenStream.peek().type == T_STRING))
		{
			locationConfig.uploadPath = tokenStream.advance().value;
			tokenStream.expect(T_SEMICOLON);
		}
		else if (token.value == "cgiEnabled")
			locationConfig.cgiEnabled = validBool(tokenStream);
		else if (token.value == "cgiExtensions" && (tokenStream.peek().type == T_WORD || tokenStream.peek().type == T_STRING))
			locationConfig.cgiExtensions = checkCgiExtensions(tokenStream); // MAP OF EXTENTIONS AND ITS ?PATHS?
		else
			throw std::runtime_error("Invalid property of location: " + token.value);
		token = tokenStream.advance();
		std::cout << "New token: " + token.value << std::endl;
	}
	tokenStream.expect(T_RBRACE);
	serverConfig.addLocation(locationConfig);
}

bool	checkPort(std::string port)
{
	for (size_t i = 0; i < port.size(); i++)
	{
		if (!isdigit(port[i]))
		{
			return false;
		}
	}
	return true;
}

void parseIP(TokenStream& tokenStream, ServerConfig& serverConfig)
{
	std::cout << "Entering parse IP! " << tokenStream.peek().value << std::endl;
	const Token& IP = tokenStream.expect(T_WORD);
	std::string host;
	std::string port;
	size_t posColon = IP.value.find_first_of(':');

	if (posColon != std::string::npos)
	{
		host = IP.value.substr(0, posColon);
		port = IP.value.substr(posColon + 1 , IP.value.size() - 2);
		serverConfig.setHost(host);
		if (checkPort(port))
		{
			int numPort = std::atoi(port.c_str());
			serverConfig.setPort(numPort);
			tokenStream.expect(T_SEMICOLON);
		}
		else
			throw std::runtime_error("Invalid port: " + port);
	}
	else
		throw std::runtime_error("Invalid IP address: " + IP.value);
}

// void parseRoot(TokenStream& tokenStream, ServerConfig& serverConfig)
// {
// 	Token root = tokenStream.expect(T_WORD);
// 	serverConfig.setRoot(root.value);
// 	tokenStream.expect(T_SEMICOLON);	
// }

void ConfigParser::parseDirective(TokenStream& tokenStream, ServerConfig& serverConfig)
{
	const Token& token = tokenStream.advance();

	std::cout << "Entering parse directive! " << tokenStream.peek().value << std::endl;
	if (token.value == "listen")
	{
		parseIP(tokenStream, serverConfig);
	}
	else if (token.value == "root")
	{
		Token root = tokenStream.expect(T_WORD);
		serverConfig.setRoot(root.value);
		tokenStream.expect(T_SEMICOLON);
	}
	else if (token.value == "index")
	{
		Token index = tokenStream.expect(T_WORD);
		serverConfig.setIndex(index.value);
		tokenStream.expect(T_SEMICOLON);
	}
	else if (token.value == "client_max_body")
	{
		Token token = tokenStream.expect(T_WORD);
		// serverConfig.setClientMaxBodySize(token.value);
		tokenStream.expect(T_SEMICOLON);		
	}
	else if (token.value == "error_page")
	{
		// parseError();
	}
	else if (token.value == "location")
	{
		parseLocation(tokenStream, serverConfig);
	}
}

void ConfigParser::parseServer(TokenStream& tokenStream, ServerConfig& serverConfig)
{
	Token token = tokenStream.expect(T_LBRACE);
	while (token.type != T_RBRACE || tokenStream.atEnd())
	{
		std::cout << "Entering parse server!" << std::endl;
		parseDirective(tokenStream, serverConfig);
		token = tokenStream.peek();
	}
}

void printServerConfig(const ServerConfig& serverConfig)
{
	std::cout << "host: " << serverConfig.getHost() << std::endl;
	std::cout << "port: " << serverConfig.getPort() << std::endl;
	std::cout << "root: " << serverConfig.getRoot() << std::endl;
	std::cout << "index: " << serverConfig.getIndex() << std::endl;
	std::cout << "clientMaxBodySize: " << serverConfig.getClientMaxBodySize() << std::endl;

	std::cout << "errorPages:" << std::endl;
	const std::map<int, std::string>& errorPages = serverConfig.getErrorpages();
	for (std::map<int, std::string>::const_iterator it = errorPages.begin(); it != errorPages.end(); ++it)
		std::cout << "  " << it->first << " -> " << it->second << std::endl;

	std::cout << "locations:" << std::endl;
	const std::vector<LocationConfig>& locations = serverConfig.getLocations();
	for (size_t i = 0; i < locations.size(); i++)
	{
		const LocationConfig& location = locations[i];

		std::cout << "  [" << i << "]" << std::endl;
		std::cout << "    path: " << location.path << std::endl;
		std::cout << "    root: " << location.root << std::endl;
		std::cout << "    index: " << location.index << std::endl;

		std::cout << "    methods:";
		for (size_t j = 0; j < location.methods.size(); j++)
			std::cout << " " << location.methods[j];
		std::cout << std::endl;

		std::cout << "    autoindex: " << location.autoindex << std::endl;
		std::cout << "    hasRedirect: " << location.hasRedirect << std::endl;
		std::cout << "    redirectCode: " << location.redirectCode << std::endl;
		std::cout << "    redirectUrl: " << location.redirectUrl << std::endl;
		std::cout << "    uploadEnabled: " << location.uploadEnabled << std::endl;
		std::cout << "    uploadPath: " << location.uploadPath << std::endl;
		std::cout << "    cgiEnabled: " << location.cgiEnabled << std::endl;

		std::cout << "    cgiExtensions:" << std::endl;
		for (std::map<std::string, std::string>::const_iterator it = location.cgiExtensions.begin();
			 it != location.cgiExtensions.end(); ++it)
			std::cout << "      " << it->first << " -> " << it->second << std::endl;
	}
}

void ConfigParser::parse()
{
	TokenStream tokenStream(_tokens);
	ServerConfig serverConfig;
	
	try 
	{
		if (tokenStream.expect(T_WORD).value == "server")
		{
			parseServer(tokenStream, serverConfig);
			printServerConfig(serverConfig);
		}
	}
	catch (std::exception& error)
	{
		std::cerr << error.what();
	}
	
}

// std::vector<ServerConfig> ConfigParser::parse()
// {
	
// }
