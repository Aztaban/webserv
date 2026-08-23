/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   TokenStream.hpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: lformank <lformank@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/08/17 17:36:45 by lformank          #+#    #+#             */
/*   Updated: 2026/08/22 17:33:40 by lformank         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef TOKENSTREAM_HPP
# define TOKENSTREAM_HPP

# include "config/Token.hpp"
# include <vector>

class TokenStream {
	private:
		size_t _pos;
		std::vector<Token> _tokens;
	public:
		TokenStream(std::vector<Token>& tokens);
		TokenStream(const TokenStream& other);
		~TokenStream();

		bool atEnd() const;
		const Token& peek() const;
		const Token& advance();
		bool match(TokenType t);
		const Token& expect(TokenType t);
};

#endif