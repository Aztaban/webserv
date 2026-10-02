#include "http/HttpRequestParser.hpp"
#include <limits>

HttpRequestParser::HttpRequestParser()
	: _request(),
	  _errorStatusCode(0),
	  _consumedBytes(0),
	  _headersComplete(false),
	  _chunkOffset(0)
{
}

HttpRequestParser::HttpRequestParser(const HttpRequestParser& other)
	: _request(other._request),
	  _errorStatusCode(other._errorStatusCode),
	  _consumedBytes(other._consumedBytes),
	  _headersComplete(other._headersComplete),
	  _chunkOffset(other._chunkOffset)
{
}

HttpRequestParser& HttpRequestParser::operator=(const HttpRequestParser& other)
{
	if (this != &other)
	{
		_request = other._request;
		_errorStatusCode = other._errorStatusCode;
		_consumedBytes = other._consumedBytes;
		_headersComplete = other._headersComplete;
		_chunkOffset = other._chunkOffset;
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

HttpRequestParser::ParseStatus
HttpRequestParser::parseChunkedBody(const std::string& rawRequest, std::size_t maxBodySize)
{
	while (true)
	{
		const std::size_t lineEnd =
			rawRequest.find("\r\n", _chunkOffset);

		if (lineEnd == std::string::npos)
			return PARSE_INCOMPLETE;

		std::string sizeLine =
			rawRequest.substr(_chunkOffset, lineEnd - _chunkOffset);

		const std::size_t semicolon = sizeLine.find(';');
		if (semicolon != std::string::npos)
			sizeLine.erase(semicolon);

		sizeLine = trim(sizeLine);
		if (!isHexDigits(sizeLine))
		{
			_errorStatusCode = 400;
			return PARSE_ERROR;
		}

		std::size_t chunkSize = 0;
		const std::size_t maximum =
			std::numeric_limits<std::size_t>::max();

		for (std::size_t i = 0; i < sizeLine.size(); ++i)
		{
			const char c = sizeLine[i];
			unsigned int digit;

			if (c >= '0' && c <= '9')
				digit = c - '0';
			else if (c >= 'a' && c <= 'f')
				digit = c - 'a' + 10;
			else
				digit = c - 'A' + 10;

			if (chunkSize > (maximum - digit) / 16)
			{
				_errorStatusCode = 400;
				return PARSE_ERROR;
			}
			chunkSize = chunkSize * 16 + digit;
		}

		if (_request.body.size() > maxBodySize
			|| chunkSize > maxBodySize - _request.body.size())
		{
			_errorStatusCode = 413;
			return PARSE_ERROR;
		}

		const std::size_t dataStart = lineEnd + 2;
		const std::size_t available = rawRequest.size() - dataStart;

		if (chunkSize == 0)
		{
			if (available >= 2
				&& rawRequest.compare(dataStart, 2, "\r\n") == 0)
			{
				_consumedBytes = dataStart + 2;
			}
			else
			{
				const std::size_t trailerEnd =
					rawRequest.find("\r\n\r\n", dataStart);

				if (trailerEnd == std::string::npos)
					return PARSE_INCOMPLETE;

				_consumedBytes = trailerEnd + 4;
			}

			_request.contentLength = _request.body.size();
			_request.hasBody = !_request.body.empty();
			return PARSE_COMPLETE;
		}

		// Wait for the complete chunk and its trailing CRLF.
		// Subtraction avoids overflowing chunkSize + 2.
		if (chunkSize > available || available - chunkSize < 2)
			return PARSE_INCOMPLETE;

		const std::size_t dataEnd = dataStart + chunkSize;
		if (rawRequest.compare(dataEnd, 2, "\r\n") != 0)
		{
			_errorStatusCode = 400;
			return PARSE_ERROR;
		}

		_request.body.append(rawRequest, dataStart, chunkSize);
		_request.contentLength = _request.body.size();
		_request.hasBody = true;

		// The next call continues here instead of decoding old chunks.
		_chunkOffset = dataEnd + 2;
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
	bool		hasHost;

	if (_request.method != "GET"
		&& _request.method != "POST"
		&& _request.method != "DELETE"
		&& _request.method != "HEAD")
	{
		_errorStatusCode = 501;
		return (PARSE_ERROR);
	}

	if (_request.version != "HTTP/1.1" && _request.version != "HTTP/1.0")
	{
		_errorStatusCode = 505;
		return (PARSE_ERROR);
	}

	if (_request.target.empty() || _request.target[0] != '/')
	{
		_errorStatusCode = 400;
		return (PARSE_ERROR);
	}

	hasHost = getHeaderValue(_request.headers, "Host", host);
	if (!hasHost && _request.version == "HTTP/1.1")
	{
		_errorStatusCode = 400;
		return (PARSE_ERROR);
	}

	if (hasHost)
	{
		std::string trimmed = host;
		while (!trimmed.empty() && (trimmed[0] == ' ' || trimmed[0] == '\t'))
			trimmed.erase(0, 1);
		while (!trimmed.empty() && (trimmed[trimmed.size() - 1] == ' ' || trimmed[trimmed.size() - 1] == '\t'))
			trimmed.erase(trimmed.size() - 1);
		if (trimmed.empty())
		{
			_errorStatusCode = 400;
			return (PARSE_ERROR);
		}
		std::size_t colon = trimmed.rfind(':');
		if (colon != std::string::npos)
		{
			if (trimmed.find(':', colon + 1) != std::string::npos)
			{
				_errorStatusCode = 400;
				return (PARSE_ERROR);
			}
			std::string port = trimmed.substr(colon + 1);
			if (port.empty() || port.find_first_not_of("0123456789") != std::string::npos)
			{
				_errorStatusCode = 400;
				return (PARSE_ERROR);
			}
		}
	}
	return (PARSE_COMPLETE);
}

//------------------------ PARSE ENTRY ------------------
HttpRequestParser::ParseStatus
HttpRequestParser::parse(const std::string& rawRequest, std::size_t maxBodySize)
{
	std::size_t	headerEnd;
	std::size_t	lineEnd;
	ParseStatus	status;
	std::string	bodyPart;
	std::string	contentLengthValue;

	if (_chunkOffset != 0)
		return parseChunkedBody(rawRequest, maxBodySize);

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

	if (!_request.isChunked && _request.contentLength > maxBodySize)
	{
		_errorStatusCode = 413;
		return PARSE_ERROR;
	}

	const std::size_t bodyStart = headerEnd + 4;
	const std::size_t availableBody = rawRequest.size() - bodyStart;

	if (!_request.isChunked
		&& _request.hasBody
		&& availableBody < _request.contentLength)
	{
		return (PARSE_INCOMPLETE);
	}

	if (_request.isChunked)
	{
		_chunkOffset = bodyStart;
		return parseChunkedBody(rawRequest, maxBodySize);
	}

	bodyPart = rawRequest.substr(bodyStart);

	if (_request.hasBody)
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
	_chunkOffset = 0;
}

bool HttpRequestParser::headersComplete() const 
{
	return _headersComplete;
}
