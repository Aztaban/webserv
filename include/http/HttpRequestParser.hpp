#ifndef HTTPREQUESTPARSER_HPP
#define HTTPREQUESTPARSER_HPP

#include "http/HttpRequest.hpp"
#include <string>

class HttpRequestParser
{
	private:
		HttpRequest _request;
		int         _errorStatusCode;

	public:
		enum ParseStatus
		{
			PARSE_INCOMPLETE,
			PARSE_COMPLETE,
			PARSE_ERROR
		};

		HttpRequestParser();

		ParseStatus         parse(const std::string &rawRequest);
		const HttpRequest&  getRequest() const;
		int                 getErrorStatusCode() const;
};

#endif