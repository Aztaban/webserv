#include "http/HttpRequestParser.hpp"

HttpRequestParser::HttpRequestParser(): _errorStatusCode(0) {}

HttpRequestParser::HttpRequestParser(const HttpRequestParser& other)
: _request(other._request), _errorStatusCode(other._errorStatusCode) {}

HttpRequestParser& HttpRequestParser::operator=(const HttpRequestParser& other)
{
	if (this != &other)
	{
		_request = other._request;
		_errorStatusCode = other._errorStatusCode;
	}
	return *this;
}

HttpRequestParser::~HttpRequestParser() {}

// ------------ HELPERS --------------
static std::string trim(const std::string& str)
{
	size_t start = str.find_first_not_of(" \t");
	if (start == std::string::npos)
		return "";

	size_t end = str.find_last_not_of(" \t");
	return str.substr(start, end - start + 1);
}

// -------------- PARSING ------------
HttpRequestParser::ParseStatus
HttpRequestParser::parseHeaders(const std::string& rawRequest, size_t pos)
{
	while (pos < rawRequest.size())
	{
		size_t nextLineEnd = rawRequest.find("\r\n", pos);

		if (nextLineEnd == std::string::npos)
			return PARSE_INCOMPLETE;

		std::string line = rawRequest.substr(pos, nextLineEnd - pos);

		if (line.empty())
			break ;
		
		size_t colon = line.find(':');

		if (colon == std::string::npos)
		{
			_errorStatusCode = 400;
			return PARSE_ERROR;
		}

		std::string key = trim(line.substr(0, colon));
		std::string value = trim(line.substr(colon + 1));

		_request.headers[key] = value;
		pos = nextLineEnd + 2;
	}

	return PARSE_COMPLETE;
}

HttpRequestParser::ParseStatus
HttpRequestParser::parseRequestLine(const std::string& requestLine)
{
	size_t firstSpace = requestLine.find(' ');
	size_t secondSpace = requestLine.find(' ', firstSpace + 1);

	if (firstSpace == std::string::npos || secondSpace == std::string::npos)
	{
		_errorStatusCode = 400;
		return PARSE_ERROR;
	}

	_request.method = requestLine.substr(0, firstSpace);
	_request.target = requestLine.substr(firstSpace + 1, secondSpace - firstSpace - 1);
	_request.version = requestLine.substr(secondSpace + 1);

	return PARSE_COMPLETE;
}

HttpRequestParser::ParseStatus
HttpRequestParser::validateRequest()
{
	//1. method check
	if (_request.method != "GET" && _request.method != "POST" && _request.method != "DELETE")
	{
		_errorStatusCode = 405;
		return PARSE_ERROR;
	}

	//2.HTTP version check
	if (_request.version != "HTTP/1.1")
	{
		_errorStatusCode = 505;
		return PARSE_ERROR;
	}

	//3. target check
	if (_request.target.empty() || _request.target[0] != '/')
	{
		_errorStatusCode = 400;
		return PARSE_ERROR;
	}

	//4. host header check (HTTP/1.1 mandatory)
	if (_request.headers.find("Host") == _request.headers.end())
	{
		_errorStatusCode = 400;
		return PARSE_ERROR;
	}

	return PARSE_COMPLETE;
}

HttpRequestParser::ParseStatus
HttpRequestParser::parse(const std::string& rawRequest)
{
	_request = HttpRequest();
	_errorStatusCode = 0;

	// 1. Find end of headers block
	size_t headerEnd = rawRequest.find("\r\n\r\n");
	if (headerEnd == std::string::npos)
		return PARSE_INCOMPLETE;

	// 2. Request line
	size_t lineEnd = rawRequest.find("\r\n");
	if (lineEnd == std::string::npos)
		return PARSE_INCOMPLETE;

	std::string requestLine = rawRequest.substr(0, lineEnd);

	ParseStatus status = parseRequestLine(requestLine);
	if (status != PARSE_COMPLETE)
		return status;

	// 3. Headers
	status = parseHeaders(rawRequest, lineEnd + 2);
	if (status != PARSE_COMPLETE)
		return status;

	// 4. Body
	std::string bodyPart = rawRequest.substr(headerEnd + 4);

	std::map<std::string, std::string>::iterator it =
		_request.headers.find("Content-Length");

	if (it != _request.headers.end())
	{
		size_t contentLength = static_cast<size_t>(std::atoi(it->second.c_str()));

		if (bodyPart.size() < contentLength)
			return PARSE_INCOMPLETE;

		_request.body = bodyPart.substr(0, contentLength);
	}
	else
	{
		_request.body = bodyPart; // optional requests
	}

	status = validateRequest();
	if (status != PARSE_COMPLETE)
		return status;

	return PARSE_COMPLETE;
}

const HttpRequest&  HttpRequestParser::getRequest() const
{
	return (_request);
}

int HttpRequestParser::getErrorStatusCode() const
{
	return _errorStatusCode;
}
