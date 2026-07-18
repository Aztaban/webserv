#include "http/HttpRequestParser.hpp"

HttpRequestParser::HttpRequestParser()
	: _request(),
	  _errorStatusCode(0),
	  _consumedBytes(0),
	  _headersComplete(false)
{}

HttpRequestParser::HttpRequestParser(const HttpRequestParser& other)
	: _request(other._request),
	  _errorStatusCode(other._errorStatusCode),
	  _consumedBytes(other._consumedBytes),
	  _headersComplete(other._headersComplete)
{}

HttpRequestParser& HttpRequestParser::operator=(const HttpRequestParser& other)
{
	if (this != &other)
	{
		_request = other._request;
		_errorStatusCode = other._errorStatusCode;
		_consumedBytes = other._consumedBytes;
		_headersComplete = other._headersComplete;
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

static std::string	toLowerString(const std::string& str)
{
	std::string	result;

	result = str;
	for (std::size_t i = 0; i < result.size(); i++)
	{
		result[i] = static_cast<char>(
			std::tolower(static_cast<unsigned char>(result[i])));
	}
	return (result);
}

static bool isDigits(const std::string& str)
{
	if (str.empty())
		return false;

	for (std::size_t i = 0; i < str.size(); i++)
	{
		if (str[i] < '0' || str[i] > '9' )
			return false;
	}
	return true;
}

static bool	isHexDigits(const std::string& str)
{
	if (str.empty())
		return (false);

	for (std::size_t i = 0; i < str.size(); i++)
	{
		if (!std::isxdigit(static_cast<unsigned char>(str[i])))
			return (false);
	}
	return (true);
}

static bool	getHeaderValue(const std::map<std::string, std::string>& headers,
							 const std::string& name,
							 std::string& value)
{
	std::string	lowerName;

	lowerName = toLowerString(name);

	for (std::map<std::string, std::string>::const_iterator it = headers.begin();
		 it != headers.end();
		 ++it)
	{
		if (toLowerString(it->first) == lowerName)
		{
			value = it->second;
			return (true);
		}
	}

	return (false);
}

static HttpRequestParser::ParseStatus	parseChunkedBody(
	const std::string& bodyPart,
	std::string& decodedBody,
	std::size_t& consumedBytes)
{
	std::size_t i = 0;

	decodedBody.clear();
	consumedBytes = 0;

	while (true)
	{
		std::size_t	lineEnd;
		std::string	sizeLine;
		std::size_t	semiColon;
		long		chunkSize;

		lineEnd = bodyPart.find("\r\n", i);
		if (lineEnd == std::string::npos)
			return HttpRequestParser::PARSE_INCOMPLETE;

		sizeLine = bodyPart.substr(i, lineEnd - i);

		semiColon = sizeLine.find(";");
		if (semiColon != std::string::npos)
			sizeLine = sizeLine.substr(0, semiColon);

		sizeLine = trim(sizeLine);
		if (!isHexDigits(sizeLine))
			return HttpRequestParser::PARSE_ERROR;
		
		chunkSize = std::strtol(sizeLine.c_str(), NULL, 16);
		if (chunkSize < 0)
			return HttpRequestParser::PARSE_ERROR;

		i = lineEnd + 2;

		if (chunkSize == 0)
		{
			std::size_t	trailerEnd;

			trailerEnd = bodyPart.find("\r\n\r\n", i);
			if (trailerEnd != std::string::npos)
			{
				consumedBytes = trailerEnd + 4;
				return HttpRequestParser::PARSE_COMPLETE;
			}

			if (bodyPart.size() >= i + 2 && bodyPart.substr(i, 2) == "\r\n")
			{
				consumedBytes = i + 2;
				return HttpRequestParser::PARSE_COMPLETE;
			}
			return HttpRequestParser::PARSE_INCOMPLETE;
		}

		if (bodyPart.size() < i + static_cast<std::size_t>(chunkSize) + 2)
			return HttpRequestParser::PARSE_INCOMPLETE;

		decodedBody.append(bodyPart.substr(i, static_cast<std::size_t>(chunkSize)));
		i += static_cast<std::size_t>(chunkSize);

		if (bodyPart.substr(i, 2) != "\r\n")
			return HttpRequestParser::PARSE_ERROR;

		i += 2;
	}
}

//------------------------ HEADERS --------------------------
HttpRequestParser::ParseStatus
HttpRequestParser::parseHeaders(const std::string& rawRequest, size_t pos)
{
	while (pos < rawRequest.size())
	{
		std::size_t	nextLineEnd = rawRequest.find("\r\n", pos);
		if (nextLineEnd == std::string::npos)
			return PARSE_INCOMPLETE;

		std::string	line = rawRequest.substr(pos, nextLineEnd - pos);

		if (line.empty())
			break;

		std::size_t	colon = line.find(':');
		if (colon == std::string::npos)
		{
			_errorStatusCode = 400;
			return PARSE_ERROR;
		}

		std::string	key = trim(line.substr(0, colon));
		std::string	value = trim(line.substr(colon + 1));

		if (key.empty())
		{
			_errorStatusCode = 400;
			return (PARSE_ERROR);
		}

		_request.headers[key] = value;

		if (toLowerString(key) == "transfer-encoding")
		{
			_request.transferEncoding = value;
			if (toLowerString(value).find("chunked") != std::string::npos)
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
	size_t	firstSpace = requestLine.find(' ');
	size_t	secondSpace = requestLine.find(' ', firstSpace + 1);

	if (firstSpace == std::string::npos || secondSpace == std::string::npos)
	{
		_errorStatusCode = 400;
		return PARSE_ERROR;
	}

	_request.method = requestLine.substr(0, firstSpace);
	_request.target = requestLine.substr(firstSpace + 1, secondSpace - firstSpace - 1);
	_request.version = requestLine.substr(secondSpace + 1);

	std::size_t	queryPos = _request.target.find('?');
	if (queryPos == std::string::npos)
	{
		_request.path = _request.target;
		_request.query = "";
	}
	else
	{
		_request.path = _request.target.substr(0, queryPos);
		_request.query = _request.target.substr(queryPos + 1);
	}

	return PARSE_COMPLETE;
}

//-------------------------------------- VALIDATION -------------------------------
HttpRequestParser::ParseStatus
HttpRequestParser::validateRequest()
{
	std::string	host;

	if (_request.method != "GET"
		&& _request.method != "POST"
		&& _request.method != "DELETE")
	{
		_errorStatusCode = 405;
		return (PARSE_ERROR);
	}

	if (_request.version != "HTTP/1.1")
	{
		_errorStatusCode = 505;
		return (PARSE_ERROR);
	}

	if (_request.target.empty() || _request.target[0] != '/')
	{
		_errorStatusCode = 400;
		return (PARSE_ERROR);
	}

	if (!getHeaderValue(_request.headers, "Host", host))
	{
		_errorStatusCode = 400;
		return (PARSE_ERROR);
	}

	return (PARSE_COMPLETE);
}

//------------------------ PARSE ENTRY ------------------
HttpRequestParser::ParseStatus
HttpRequestParser::parse(const std::string& rawRequest)
{
	std::size_t	headerEnd;
	std::size_t	lineEnd;
	ParseStatus	status;
	std::string	bodyPart;
	std::string	contentLengthValue;

	reset();

	headerEnd = rawRequest.find("\r\n\r\n");
	if (headerEnd == std::string::npos)
		return (PARSE_INCOMPLETE);

	lineEnd = rawRequest.find("\r\n");
	if (lineEnd == std::string::npos)
		return (PARSE_INCOMPLETE);

	std::string	requestLine = rawRequest.substr(0, lineEnd);

	status = parseRequestLine(requestLine);
	if (status != PARSE_COMPLETE)
		return (status);

	status = parseHeaders(rawRequest, lineEnd + 2);
	if (status != PARSE_COMPLETE)
		return (status);

	_headersComplete = true;

	if (_request.isChunked)
	{
		if (getHeaderValue(_request.headers, "Content-Length",
				contentLengthValue))
		{
			_errorStatusCode = 400;
			return (PARSE_ERROR);
		}
	}
	else if (getHeaderValue(_request.headers, "Content-Length",
			contentLengthValue))
	{
		std::size_t	contentLength;

		if (!isDigits(contentLengthValue))
		{
			_errorStatusCode = 400;
			return (PARSE_ERROR);
		}

		contentLength = static_cast<std::size_t>(
			std::strtoul(contentLengthValue.c_str(), NULL, 10));

		_request.hasBody = (contentLength > 0);
		_request.contentLength = contentLength;
	}
	else
	{
		_request.hasBody = false;
		_request.contentLength = 0;
	}

	status = validateRequest();
	if (status != PARSE_COMPLETE)
		return (status);

	bodyPart = rawRequest.substr(headerEnd + 4);

	if (_request.isChunked)
	{
		std::string	decodedBody;
		std::size_t	chunkedConsumed;

		status = parseChunkedBody(bodyPart, decodedBody, chunkedConsumed);
		if (status == PARSE_INCOMPLETE)
			return (PARSE_INCOMPLETE);

		if (status == PARSE_ERROR)
		{
			_errorStatusCode = 400;
			return (PARSE_ERROR);
		}

		_request.hasBody = (!decodedBody.empty());
		_request.body = decodedBody;
		_request.contentLength = decodedBody.size();
		_consumedBytes = headerEnd + 4 + chunkedConsumed;
	}
	else if (_request.hasBody)
	{
		if (bodyPart.size() < _request.contentLength)
			return (PARSE_INCOMPLETE);

		_request.body = bodyPart.substr(0, _request.contentLength);
		_consumedBytes = headerEnd + 4 + _request.contentLength;
	}
	else
	{
		_request.body = "";
		_consumedBytes = headerEnd + 4;
	}

	return (PARSE_COMPLETE);
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

std::size_t	HttpRequestParser::getConsumedBytes() const
{
	return _consumedBytes;
}

void HttpRequestParser::reset()
{
	_request = HttpRequest();
	_errorStatusCode = 0;
	_consumedBytes = 0;
	_headersComplete = 0;
}

bool HttpRequestParser::headersComplete() const 
{
	return _headersComplete;
}