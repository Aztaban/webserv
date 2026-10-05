#include "cgi/CgiOutputParser.hpp"
#include "http/HttpHeaderUtils.hpp"

namespace
{
	HttpResponse invalidOutput()
	{
		return HttpResponse(500, "Internal Server Error", "Invalid CGI response headers");
	}

	bool validHeaderName(const std::string& name)
	{
		if (name.empty()) return false;
		for (std::size_t i = 0; i < name.size(); ++i)
		{
			unsigned char c = static_cast<unsigned char>(name[i]);
			if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
				|| (c >= '0' && c <= '9')
				|| std::string("!#$%&'*+-.^_`|~").find(c) != std::string::npos))
				return false;
		}
		return true;
	}
}

HttpResponse CgiOutputParser::parse(std::string& rawOutput)
{
	HttpResponse response;
	std::size_t pos = 0;
	bool hasContentType = false;
	bool hasStatus = false;

	// Consume complete lines, accepting either LF or CRLF from the CGI.
	while (true)
	{
		std::size_t end = rawOutput.find('\n', pos);
		if (end == std::string::npos)
			return invalidOutput();
		std::string line = rawOutput.substr(pos, end - pos);
		if (!line.empty() && line[line.size() - 1] == '\r')
			line.erase(line.size() - 1);
		pos = end + 1;
		if (line.empty())
			break;
		std::size_t colon = line.find(':');
		if (colon == std::string::npos || !validHeaderName(line.substr(0, colon)))
			return invalidOutput();
		std::string name = line.substr(0, colon);
		std::string lower = HttpHeaderUtils::lowercase(name);
		std::string value = HttpHeaderUtils::trimOws(line.substr(colon + 1));
		for (std::size_t i = 0; i < value.size(); ++i)
		{
			unsigned char c = static_cast<unsigned char>(value[i]);
			if ((c < 32 && c != '\t') || c == 127)
				return invalidOutput();
		}
		if (lower == "status")
		{
			if (hasStatus || value.size() < 5 || value[3] != ' '
				|| value[0] < '2' || value[0] > '5'
				|| value[1] < '0' || value[1] > '9'
				|| value[2] < '0' || value[2] > '9')
				return invalidOutput();
			response.statusCode = (value[0] - '0') * 100 + (value[1] - '0') * 10 + value[2] - '0';
			response.reasonPhrase = value.substr(4);
			hasStatus = true;
		}
		else if (lower == "content-type")
		{
			if (hasContentType || value.empty())
				return invalidOutput();
			response.headers["Content-Type"] = value;
			hasContentType = true;
		}
		else if (lower != "content-length" && lower != "connection" && lower != "transfer-encoding")
			response.headers[name] = value;
	}
	if (!hasContentType)
		return invalidOutput();
	// The server collects CGI stdout through EOF and owns HTTP framing.
	// Drop the small CGI header prefix in place and transfer ownership of the
	// potentially huge body instead of allocating/copying it again.
	rawOutput.erase(0, pos);
	response.body.swap(rawOutput);
	return response;
}
