/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   TokenStream.cpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: lformank <lformank@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/08/17 17:36:57 by lformank          #+#    #+#             */
/*   Updated: 2026/08/23 11:58:52 by lformank         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "config/TokenStream.hpp"
#include "config/Token.hpp"
#include <vector>
#include <iostream>

TokenStream::TokenStream(std::vector<Token>& tokens) : _pos(0), _tokens(tokens)
{}

TokenStream::TokenStream(const TokenStream& other)
{
	this->_tokens.clear();
	this->_tokens.insert(this->_tokens.begin(), other._tokens.begin(), other._tokens.end());
	this->_pos = other._pos;
}

TokenStream::~TokenStream() {}

bool TokenStream::atEnd() const
{
	return _pos >= _tokens.size();
}

const Token& TokenStream::peek() const
{
	if (!atEnd())
		return _tokens[_pos];
	else
		throw std::runtime_error("Out of _tokens' bounds");
}

const Token& TokenStream::advance()
{
	if (!atEnd())
		return _tokens[_pos++];
	else
		throw std::runtime_error("Out of _tokens' bounds");
}

bool TokenStream::match(TokenType t)
{
	if (peek().type == t)
	{
		advance();
		return true;
	}
	return false;
}

static std::string tokenTypeName(TokenType t)
{
	switch (t)
	{
		case T_WORD:      return "word";
		case T_LBRACE:    return "lbrace";
		case T_RBRACE:    return "rbrace";
		case T_SEMICOLON: return "semicolon";
		case T_COMMENT:   return "comment";
		case T_STRING:    return "string";
		case T_EOF:    return "eof";
	}
	return "unknown";
}

const Token& TokenStream::expect(TokenType t)
{
	if (peek().type == t)
		return advance();
	throw std::runtime_error("Wrong syntax of config file - expected " + tokenTypeName(t)
		+ ", got: \"" + peek().value + "\" (" + tokenTypeName(peek().type) + ")\n");
}
