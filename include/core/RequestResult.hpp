/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   RequestResult.hpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: lformank <lformank@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/10 22:38:03 by lformank          #+#    #+#             */
/*   Updated: 2026/09/10 22:38:05 by lformank         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef REQUESTRESULT_HPP
# define REQUESTRESULT_HPP

# include "http/HttpResponse.hpp"
# include "cgi/CgiRequest.hpp"

/**
 * @brief Result of request dispatching.
 *
 * RequestResult allows RequestHandler to either return a ready HttpResponse
 * immediately, or ask the EventLoop to start a CGI job.
 *
 * This is mainly used for future non-blocking CGI integration.
 */
class RequestResult {

public:
	enum Type {
		RESPONSE_READY,
		CGI_REQUIRED
	};

	Type			type;
	HttpResponse	response;
	CgiRequest		cgiRequest;

	RequestResult();

	static RequestResult	fromResponse(const HttpResponse& response);
	static RequestResult	fromCgiRequest(const CgiRequest& cgiRequest);

};

#endif