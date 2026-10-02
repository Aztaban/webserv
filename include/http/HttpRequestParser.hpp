#ifndef HTTPREQUESTPARSER_HPP
#define HTTPREQUESTPARSER_HPP

#include "http/HttpRequest.hpp"
#include <string>
#include <cstdlib>
#include <cctype>
#include <limits>

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
		HttpRequest	_request;
		int			_errorStatusCode;
		std::size_t	_consumedBytes;
		bool		_headersComplete;
		std::size_t _chunkOffset;
		ParseStatus parseChunkedBody(
		const std::string& rawRequest,
		std::size_t maxBodySize);


		ParseStatus	parseRequestLine(const std::string& requestLine);
		ParseStatus	parseHeaders(const std::string& rawRequest, size_t startPos);
		ParseStatus	validateRequest();

	public:
		HttpRequestParser();
		HttpRequestParser(const HttpRequestParser& other);
		HttpRequestParser& operator=(const HttpRequestParser& other);
		~HttpRequestParser();

		ParseStatus parse(
		const std::string& rawRequest,
		std::size_t maxBodySize =
		std::numeric_limits<std::size_t>::max());

		const HttpRequest&	getRequest() const;
		int					getErrorStatusCode() const;
		bool				headersComplete() const;
		std::size_t			getConsumedBytes() const;
		void 				reset();
	};

#endif