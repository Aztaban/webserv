/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   CgiOutputParser.cpp                                :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: lformank <lformank@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/06 17:43:44 by lformank          #+#    #+#             */
/*   Updated: 2026/09/13 14:56:38 by lformank         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "cgi/CgiOutputParser.hpp"
#include <map>
#include <cstdlib>
#include <cctype>

void checkStatusBody(HttpResponse& response, const std::string& body)
{
	size_t space;

	if (body.find(' ') != std::string::npos)
	{
		std::string num;
		std::string reason;

		space = body.find(' ');
		num = body.substr(0, space);
		reason = body.substr(space + 1, body.length() - space);

		for (size_t i = 0; i < num.size(); i++)
		{
			if (!isdigit(num[i]))
				return;
		}

		response.statusCode = std::strtol(num.c_str(), NULL, 0);
		response.reasonPhrase = reason;
	}
}

void saveHeader(const std::string& rawOutput, HttpResponse& response, size_t& i)
{
	size_t delimiter = rawOutput.find(":", i);
	size_t endBody = std::string::npos;

	if (rawOutput.find("\r\n", delimiter) != std::string::npos)
		endBody = rawOutput.find("\r\n", delimiter);
	else if (rawOutput.find('\n', delimiter) != std::string::npos)
		endBody = rawOutput.find('\n', delimiter);
	
	std::string header = rawOutput.substr(i, delimiter - i);
	std::string body = rawOutput.substr(delimiter + 2, endBody - (delimiter + 2));

	if (header == "Status")
		checkStatusBody(response, body);
	else
		response.headers.insert(std::make_pair(header, body));
	i = endBody;
}

size_t findHeadersDelimiter(const std::string& rawOutput, size_t& delimiterLength)
{
	if (rawOutput.find("\r\n\r\n") != std::string::npos)
	{
		delimiterLength = 4;
		return (rawOutput.find("\r\n\r\n"));
	}
	if (rawOutput.find("\n\n") != std::string::npos)
	{
		delimiterLength = 2;
		return (rawOutput.find("\n\n"));
	}
	delimiterLength = 0;
	return std::string::npos;
}

HttpResponse CgiOutputParser::parse(const std::string& rawOutput)
{
	HttpResponse response;
	size_t delimiterLength;
	size_t index = findHeadersDelimiter(rawOutput, delimiterLength);

	for (size_t i = 0; index != std::string::npos && i < index; i++)
	{
		size_t colon = rawOutput.find(":", i);
		if (colon != std::string::npos && colon < index)
		{
			saveHeader(rawOutput, response, i);
		}
		else
			break;
	}
	if (index == std::string::npos)
		response.body = rawOutput;
	else
		response.body = rawOutput.substr(index + delimiterLength);
	if (response.headers.find("Content-Type") == response.headers.end())
	{
		return HttpResponse(500, "Internal Server Error", "CGI output missing Content-Type header");
	}
	
	return response;
}