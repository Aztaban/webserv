#include "http/HttpResponseBuilder.hpp"

#include <sstream>

std::string HttpResponseBuilder::build(const HttpResponse& response)
{
	std::ostringstream	raw;

	raw	<< "HTTP/1.1 "
		<< response.statusCode
		<< " "
		<< response.reasonPhrase
		<< "\r\n";

	std::map<std::string, std::string>::const_iterator it;
	for (it = response.headers.begin(); it != response.headers.end(); it++)
	{
		raw << it->first << ": " << it->second << "\r\n";
	}

	if (response.headers.find("Content-length") == response.headers.end())
		raw << "Content-Length: " << response.body.size() << "\r\n";

	if (response.headers.find("Content-Type") == response.headers.end())
		raw << "Content-Type: text/plain\r\n";

	if (response.headers.find("Connection") == response.headers.end())
	{
		if (response.closeConnection)
			raw << "Connection: close\r\n";
		else
			raw << "Connection: keep-alive\r\n";
	}
	
	raw << "\r\n";
	raw << response.body;

	return raw.str();
}

std::string HttpResponseBuilder::buildSimpleResponse(int statusCode, const std::string& reasonPhrase, const std::string& body)
{
	HttpResponse response(statusCode, reasonPhrase, body);
	return build(response);
}
