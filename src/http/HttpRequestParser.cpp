#include "http/HttpRequestParser.hpp"

//-------------------------- CONSTRUCTORS ---------------------------
HttpRequestParser::HttpRequestParser()
	: _errorStatusCode(0) {}

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

//----------------------- HELPERS ---------------------------
static std::string trim(const std::string& str)
{
	size_t start = str.find_first_not_of(" \t");
	if (start == std::string::npos)
		return "";

	size_t end = str.find_last_not_of(" \t");
	return str.substr(start, end - start + 1);
}

static std::string parseChunkedBody(const std::string& body)
{
	std::string result;
	size_t i = 0;

	while (i < body.size())
	{
		size_t lineEnd = body.find("\r\n", i);
		if (lineEnd == std::string::npos)
			break;

		std::string sizeStr = body.substr(i, lineEnd - i);
		size_t chunkSize = std::strtol(sizeStr.c_str(), NULL, 16);

		i = lineEnd + 2;

		if (chunkSize == 0)
			break;

		if (i + chunkSize > body.size())
			break;

		result.append(body.substr(i, chunkSize));
		i += chunkSize + 2;
	}

	return result;
}

//------------------------ HEADERS --------------------------
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
			break;

		size_t colon = line.find(':');
		if (colon == std::string::npos)
		{
			_errorStatusCode = 400;
			return PARSE_ERROR;
		}

		std::string key = trim(line.substr(0, colon));
		std::string value = trim(line.substr(colon + 1));

		_request.headers[key] = value;

		if (key == "Transfer-Encoding")
		{
			_request.transferEncoding = value;
			if (value.find("chunked") != std::string::npos)
				_request.isChunked = true;
		}

		pos = nextLineEnd + 2;
	}

	return PARSE_COMPLETE;
}

//------------------------ REQUEST LINE -----------------------------
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

//-------------------------------------- VALIDATION -------------------------------
HttpRequestParser::ParseStatus
HttpRequestParser::validateRequest()
{
	if (_request.method != "GET" &&
		_request.method != "POST" &&
		_request.method != "DELETE")
	{
		_errorStatusCode = 405;
		return PARSE_ERROR;
	}

	if (_request.version != "HTTP/1.1")
	{
		_errorStatusCode = 505;
		return PARSE_ERROR;
	}

	if (_request.target.empty() || _request.target[0] != '/')
	{
		_errorStatusCode = 400;
		return PARSE_ERROR;
	}

	if (_request.headers.find("Host") == _request.headers.end())
	{
		_errorStatusCode = 400;
		return PARSE_ERROR;
	}

	return PARSE_COMPLETE;
}

//------------------------ PARSE ENTRY ------------------
HttpRequestParser::ParseStatus
HttpRequestParser::parse(const std::string& rawRequest)
{
	_request = HttpRequest();
	_errorStatusCode = 0;

	size_t headerEnd = rawRequest.find("\r\n\r\n");
	if (headerEnd == std::string::npos)
		return PARSE_INCOMPLETE;

	size_t lineEnd = rawRequest.find("\r\n");
	if (lineEnd == std::string::npos)
		return PARSE_INCOMPLETE;

	std::string requestLine = rawRequest.substr(0, lineEnd);

	ParseStatus status = parseRequestLine(requestLine);
	if (status != PARSE_COMPLETE)
		return status;

	status = parseHeaders(rawRequest, lineEnd + 2);
	if (status != PARSE_COMPLETE)
		return status;

	std::string bodyPart = rawRequest.substr(headerEnd + 4);

	if (_request.isChunked)
	{
		_request.body = parseChunkedBody(bodyPart);
	}
	else
	{
		std::map<std::string, std::string>::iterator it =
			_request.headers.find("Content-Length");

		if (it != _request.headers.end())
		{
			size_t contentLength = std::atoi(it->second.c_str());

			if (bodyPart.size() < contentLength)
				return PARSE_INCOMPLETE;

			_request.body = bodyPart.substr(0, contentLength);
		}
		else
		{
			_request.body = bodyPart;
		}
	}

	status = validateRequest();
	if (status != PARSE_COMPLETE)
		return status;

	return PARSE_COMPLETE;
}

//-------------------------- GETTERS ----------------------------------
const HttpRequest& HttpRequestParser::getRequest() const
{
	return _request;
}

int HttpRequestParser::getErrorStatusCode() const
{
	return _errorStatusCode;
}