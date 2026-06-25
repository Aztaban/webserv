#ifndef HTTPREQUESTPARSER_HPP
#define HTTPREQUESTPARSER_HPP

#include "http/HttpRequest.hpp"
#include <string>
#include <cstdlib>
#include <cstdlib>

class HttpRequestParser
{
	public:
		enum ParseStatus
		{
			PARSE_INCOMPLETE,
			PARSE_COMPLETE,
			PARSE_ERROR
		};

	private:
		HttpRequest _request;
		int _errorStatusCode;

		ParseStatus parseRequestLine(const std::string& requestLine);
		ParseStatus parseHeaders(const std::string& rawRequest, size_t startPos);
		ParseStatus validateRequest();

	public:
		HttpRequestParser();
		HttpRequestParser(const HttpRequestParser& other);
		HttpRequestParser& operator=(const HttpRequestParser& other);
		~HttpRequestParser();

		ParseStatus parse(const std::string& rawRequest);

		const HttpRequest& getRequest() const;
		int getErrorStatusCode() const;
	};

#endif