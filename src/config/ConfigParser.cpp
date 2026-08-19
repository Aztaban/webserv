/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ConfigParser.cpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: lformank <lformank@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/08/13 13:21:53 by lformank          #+#    #+#             */
/*   Updated: 2026/08/19 21:00:49 by lformank         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "config/ConfigParser.hpp"

#include <fstream>
#include <sstream>
#include <iostream>
#include <string>
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
	for (size_t i = 0; i < _tokens.size(); i++)
	{
		std::cout << "Type: " << _tokens[i].type << "\tValue: " << _tokens[i].value << std::endl;
	}
}

void parseLocation(TokenStream& tokenStream, ServerConfig& serverConfig)
{
	Token token = tokenStream.expect(T_WORD);
	LocationConfig locationConfig;

	if (token.value[0] != '/')
		throw std::runtime_error("Invalid location's address: " + token.value);
	locationConfig.path = token.value;
	tokenStream.expect(T_LBRACE);
	token = tokenStream.expect(T_WORD);
	if (token.value == "root")
	{
		locationConfig.root = token.value;
	}
	else if (token.value == "index")
	{
		locationConfig.index = token.value;
	}
	else if (token.value == "methods")
	{
		locationConfig.methods = token.value; // VECTOR OF METHODS
	}
	else if (token.value == "autoindex")
	{
		if (tokenStream.expect(T_WORD).value == "true")
			locationConfig.autoindex = true;
	}
	else if (token.value == "hasRedirect")
	{
		if (tokenStream.expect(T_WORD).value == "true")
			locationConfig.hasRedirect = true;
	}
	else if (token.value == "redirectCode")
	{
		if (tokenStream.expect(T_WORD).value == "true")
			locationConfig.redirectCode = true;
	}
	else if (token.value == "redirectUrl")
	{
		locationConfig.redirectUrl = token.value;
	}
	else if (token.value == "uploadEnabled")
	{
		if (tokenStream.expect(T_WORD).value == "true")
			locationConfig.uploadEnabled = true;
	}
	else if (token.value == "uploadPath")
	{
		locationConfig.uploadPath = token.value;
	}
	else if (token.value == "cgiEnabled")
	{
		if (tokenStream.expect(T_WORD).value == "true")
			locationConfig.cgiEnabled = true;	
	}
	else if (token.value == "cgiExtensions")
	{
		locationConfig.cgiExtensions = token.value; // MAP OF EXTENTIONS AND ITS ?PATHS?
	}
	else
		throw std::runtime_error("Invalid property of location: " + token.value);
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
	const Token& IP = tokenStream.expect(T_WORD);
	std::string host;
	std::string port;
	size_t posSemicolon = IP.value.find_first_of(':');

	if (posSemicolon != std::string::npos)
	{
		host = IP.value.substr(0, posSemicolon);
		port = IP.value.substr(posSemicolon + 1 , IP.value.size() - 2);
		serverConfig.setHost(host);
		if (checkPort(port))
		{
			int numPort = atoi(port.c_str());
			serverConfig.setPort(numPort);
		}
		else
			throw std::runtime_error("Invalid port: " + port);
	}
	else
		throw std::runtime_error("Invalid IP address: " + IP.value);
}

void ConfigParser::parseDirective(TokenStream& tokenStream, ServerConfig& serverConfig)
{
	const Token& token = tokenStream.advance();

	if (token.value == "listen")
	{
		parseIP(tokenStream, serverConfig);
	}
	else if (token.value == "server_name")
	{
	
	}
	else if (token.value == "index")
	{
		
	}
	else if (token.value == "client_max_body")
	{
		
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
	tokenStream.expect(T_LBRACE);
	if (tokenStream.peek().type == T_WORD)
	{
		parseDirective(tokenStream, serverConfig);
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
