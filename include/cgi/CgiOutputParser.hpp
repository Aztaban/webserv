/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   CgiOutputParser.hpp                                :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: lformank <lformank@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/06 17:43:21 by lformank          #+#    #+#             */
/*   Updated: 2026/09/06 17:43:23 by lformank         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CGIOUTPUTPARSER
# define CGIOUTPUTPARSER

# include "http/HttpResponse.hpp"

# include <string>

class CgiOutputParser {

	public:
		static HttpResponse parse(const std::string& rawOutput);

};

#endif