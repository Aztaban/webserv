/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Token.hpp                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: lformank <lformank@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/08/17 17:38:30 by lformank          #+#    #+#             */
/*   Updated: 2026/08/17 17:42:08 by lformank         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef TOKEN_HPP
# define TOKEN_HPP

# include <string>

enum TokenType {
	T_WORD,
	T_LBRACE,
	T_RBRACE,
	T_SEMICOLON,
	T_COMMENT,
	T_STRING
};

struct Token {
	TokenType	type;
	std::string	value;
};

#endif