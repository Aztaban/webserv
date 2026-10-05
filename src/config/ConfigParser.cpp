#include "utils/Logger.hpp"
/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ConfigParser.cpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: lformank <lformank@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/08/13 13:21:53 by lformank          #+#    #+#             */
/*   Updated: 2026/08/26 17:54:57 by lformank         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "config/ConfigParser.hpp"

#include <fstream>
#include <sstream>
#include <iostream>
#include <string>
#include <cstdlib>
#include <climits>
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
			}
			else
				word += _source[i];
		}
		else
			pushWord(word);
	}
	pushWord(word);
	pushToken("", T_EOF);
}

bool validBool(TokenStream& tokenStream)
{
	Token token = tokenStream.expect(T_WORD);

	tokenStream.expect(T_SEMICOLON);
	if (token.value == "on")
		return true;
	else if (token.value != "off")
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

std::vector<std::string> checkMethods(TokenStream& tokenStream)
{
	Token token = tokenStream.advance();
	std::vector<std::string> methods;

	while (token.type != T_SEMICOLON && token.type != T_RBRACE)
	{
		if (token.value == "GET" || token.value == "POST" || token.value == "DELETE")
		{
			methods.push_back(token.value);
			token = tokenStream.advance();
		}
		else
			throw std::runtime_error("Invalid method: " + token.value);
	}
	if (token.type != T_SEMICOLON)
		throw std::runtime_error("Missing ';' after methods");
	return methods;
}

std::map<std::string, std::string> checkCgiExtensions(TokenStream& tokenStream)
{
	std::map<std::string, std::string> extensions;
	Token token = tokenStream.advance();

	while (token.type != T_SEMICOLON && token.type != T_RBRACE)
	{
		if (token.value.empty() || token.value[0] != '.')
			throw std::runtime_error("Invalid extension: " + token.value);
		std::string extension = token.value;

		token = tokenStream.advance();
		if (token.type != T_WORD && token.type != T_STRING)
			throw std::runtime_error("Missing path for extension: " + extension);
		extensions[extension] = token.value;

		token = tokenStream.advance();
	}
	if (token.type != T_SEMICOLON)
		throw std::runtime_error("Missing ';' after cgiExtensions");
	return extensions;
}

bool	checkNumber(std::string numStr, long min, long max)
{
	errno = 0;
	char *end;
	long num = std::strtol(numStr.c_str(), &end, 10);

	if (*end != '\0')
		return false;
	if (errno == ERANGE)
		return false;
	if (num < min || num > max)
		return false;
	return true;
}

int checkRedirectCode(TokenStream& tokenStream)
{
	Token token = tokenStream.expect(T_WORD);

	if (!checkNumber(token.value, 300, 399))
		throw std::runtime_error("Invalid redirect code: " + token.value);
	return std::atoi(token.value.c_str());
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
		else if (token.value == "return")
		{
			locationConfig.hasRedirect = true;
			locationConfig.redirectCode = checkRedirectCode(tokenStream);
			locationConfig.redirectUrl = checkRedirectUrl(tokenStream);
		}
		else if (token.value == "upload" )
			locationConfig.uploadEnabled = validBool(tokenStream);
		else if (token.value == "upload_path" && (tokenStream.peek().type == T_WORD || tokenStream.peek().type == T_STRING))
		{
			locationConfig.uploadPath = tokenStream.advance().value;
			tokenStream.expect(T_SEMICOLON);
		}
		else if (token.value == "cgi_extension" && (tokenStream.peek().type == T_WORD || tokenStream.peek().type == T_STRING))
		{
			locationConfig.cgiEnabled = true;
			locationConfig.cgiExtensions = checkCgiExtensions(tokenStream);
		}
		else if (token.value == "client_max_body_size")
		{
			Token size = tokenStream.expect(T_WORD);
			if (!checkNumber(size.value, 0, LONG_MAX))
				throw std::runtime_error("Invalid 'client_max_body_size' number\n");
			locationConfig.clientMaxBodySize = std::strtoul(size.value.c_str(), NULL, 10);
			locationConfig.hasClientMaxBodySize = true;
			tokenStream.expect(T_SEMICOLON);
		}
		else
			throw std::runtime_error("Invalid property of location: " + token.value);
		token = tokenStream.advance();
	}
	serverConfig.addLocation(locationConfig);
}

void parseError(TokenStream& tokenStream, ServerConfig& serverConfig)
{
	Token token = tokenStream.expect(T_WORD);
	long int errorNum;

	if (!checkNumber(token.value, 400, 599))
		throw std::runtime_error("Invalid error code: " + token.value);
	errorNum = std::atol(token.value.c_str());
	token = tokenStream.expect(T_WORD);
	std::string error = token.value;
	tokenStream.expect(T_SEMICOLON);
	serverConfig.addErrorPage(errorNum, error);
}

void parseIP(TokenStream& tokenStream, ServerConfig& serverConfig)
{
	const Token& IP = tokenStream.expect(T_WORD);
	std::string host;
	std::string port;
	size_t posColon = IP.value.find_first_of(':');

	if (posColon != std::string::npos)
	{
		host = IP.value.substr(0, posColon);
		port = IP.value.substr(posColon + 1);
		serverConfig.setHost(host);
		if (checkNumber(port, 1, 65535))
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



void ConfigParser::parseDirective(TokenStream& tokenStream, ServerConfig& serverConfig, std::set<std::string>& seenDirectives)
{
	Token token = tokenStream.expect(T_WORD);
	bool isSingular = (token.value == "listen" || token.value == "root" || token.value == "index"
		|| token.value == "client_max_body_size" || token.value == "server_name");

	if (isSingular && !seenDirectives.insert(token.value).second)
		throw std::runtime_error("Duplicate '" + token.value + "' directive");

	if (token.value == "listen")
		parseIP(tokenStream, serverConfig);
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
	else if (token.value == "client_max_body_size")
	{
		Token token = tokenStream.expect(T_WORD);
		if (checkNumber(token.value, 0, LONG_MAX))
			serverConfig.setClientMaxBodySize(std::strtoul(token.value.c_str(), NULL, 10));
		else
			throw std::runtime_error("Invalid 'client_max_body_size' number\n");
		tokenStream.expect(T_SEMICOLON);
	}
	else if (token.value == "error_page")
		parseError(tokenStream, serverConfig);
	else if (token.value == "location")
		parseLocation(tokenStream, serverConfig);
	else if (token.value == "server_name")
	{
		token = tokenStream.advance();
		if (token.type == T_WORD || token.type == T_STRING)
			serverConfig.setName(token.value);
		token = tokenStream.expect(T_SEMICOLON);
	}
	else
		throw std::runtime_error("Unknown directive: " + token.value);
}

void ConfigParser::parseServer(TokenStream& tokenStream, ServerConfig& serverConfig)
{
	Token token = tokenStream.expect(T_LBRACE);
	std::set<std::string> seenDirectives;
	while (token.type != T_RBRACE && !tokenStream.atEnd())
	{
		parseDirective(tokenStream, serverConfig, seenDirectives);
		token = tokenStream.peek();
	}
	tokenStream.expect(T_RBRACE);
}

static void   validateUniqueServers(const std::vector<ServerConfig>& servers)
{
	for (std::size_t i = 0; i < servers.size(); i++)
	{
		for (std::size_t j = i + 1; j < servers.size(); j++)
		{
			if (servers[i].getHost() == servers[j].getHost() && servers[i].getPort() == servers[j].getPort()
				&& servers[i].getName() == servers[j].getName())
			{
				std::ostringstream      message;
				message << "duplicate server block for " << servers[i].getHost()
					<< ":" << servers[i].getPort() << " (server_name " << servers[i].getName() << ")";
				throw std::runtime_error(message.str());
			}
		}
	}
}

std::vector<ServerConfig> ConfigParser::parse()
{
	TokenStream tokenStream(_tokens);
	std::vector<ServerConfig> vectorServerConfig;

	while (!tokenStream.atEnd() && tokenStream.peek().type == T_WORD
		&& tokenStream.expect(T_WORD).value == "server")
	{
		ServerConfig serverConfig;
		parseServer(tokenStream, serverConfig);
		vectorServerConfig.push_back(serverConfig);
	}
	validateUniqueServers(vectorServerConfig);
	return vectorServerConfig;
}
