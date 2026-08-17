/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ConfigParser.hpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: lformank <lformank@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/08/13 13:20:26 by lformank          #+#    #+#             */
/*   Updated: 2026/08/17 17:40:30 by lformank         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CONFIG_PASER_HPP
# define CONFIG_PASER_HPP

# include "config/ServerConfig.hpp"
# include "config/Token.hpp"

# include <string>
# include <vector>



class ConfigParser {
	private:
		std::vector<Token>	_tokens;
		std::string			_path;
		std::string			_source;
	public:
		ConfigParser();
		ConfigParser(const std::string& path);
		ConfigParser(const ConfigParser& other);
		const ConfigParser& operator=(const ConfigParser& other);
		~ConfigParser();
		const std::string& getSource();
		void pushToken(const std::string& str, TokenType type);
		void pushWord(std::string& word);
		void loadSource();
		void tokenize();
		void parse();
};


#endif