/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ConfigParser.cpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: lformank <lformank@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/08/13 13:21:53 by lformank          #+#    #+#             */
/*   Updated: 2026/08/17 17:41:52 by lformank         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "config/ConfigParser.hpp"

#include <fstream>
#include <sstream>
#include <iostream>
#include "config/Token.hpp"

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



void ConfigParser::parse()
{
}

// std::vector<ServerConfig> ConfigParser::parse()
// {
	
// }
