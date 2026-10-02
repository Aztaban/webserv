#include "http/HttpResponseBuilder.hpp"
#include "http/HttpHeaderUtils.hpp"

#include <sstream>

std::string HttpResponseBuilder::build(const HttpResponse& response, bool headOnly)
{
	std::ostringstream	raw;
	const bool noContent = response.statusCode == 204;

	raw	<< "HTTP/1.1 "
		<< response.statusCode
		<< " "
		<< response.reasonPhrase
		<< "\r\n";

	std::map<std::string, std::string>::const_iterator it;
	for (it = response.headers.begin(); it != response.headers.end(); it++)
	{
		const std::string name = HttpHeaderUtils::lowercase(it->first);
		if (noContent && (name == "content-length" || name == "transfer-encoding"))
			continue;
		if (it->first != "Content-Length"
			&& it->first != "Connection")
		{
			raw << it->first << ": " << it->second << "\r\n";
		}
	}

	if (!noContent && !headOnly)
		raw << "Content-Length: " << response.body.size() << "\r\n";

	if (response.closeConnection)
		raw << "Connection: close\r\n";
	else
		raw << "Connection: keep-alive\r\n";
	
	if (!noContent && response.headers.find("Content-Type") == response.headers.end())
		raw << "Content-Type: text/plain\r\n";

	raw << "\r\n";
	if (!noContent && !headOnly)
		raw << response.body;

	return (raw.str());
}

std::string HttpResponseBuilder::buildSimpleResponse(int statusCode, const std::string& reasonPhrase, const std::string& body)
{
	HttpResponse response(statusCode, reasonPhrase, body);
	
	response.headers["Content-Type"] = "text/plain";
	response.closeConnection = true;

	return (HttpResponseBuilder::build(response));
}
