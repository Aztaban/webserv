/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   RequestResult.cpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: lformank <lformank@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/10 22:44:16 by lformank          #+#    #+#             */
/*   Updated: 2026/09/10 22:44:24 by lformank         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "core/RequestResult.hpp"

RequestResult::RequestResult() : type(RESPONSE_READY), response(), cgiRequest()
{}

RequestResult	RequestResult::fromResponse(const HttpResponse& response)
{
	RequestResult	result;

	result.type = RESPONSE_READY;
	result.response = response;
	return result;
}

RequestResult	RequestResult::fromCgiRequest(const CgiRequest& cgiRequest)
{
	RequestResult	result;

	result.type = CGI_REQUIRED;
	result.cgiRequest = cgiRequest;
	return result;
}