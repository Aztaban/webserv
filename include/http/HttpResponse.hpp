#ifndef HTTPRESPONSE_HPP
#define HTTPRESPONSE_HPP

#include <string>
#include <map>

//store informations only, it contains everything needed to send back to browser:
struct HttpResponse
{
	int statusCode;
	std::string reasonPhrase;
	std::map<std::string, std::string> headers;
	std::string body;
	bool closeConnection;

	HttpResponse()
		: statusCode(200),
		  reasonPhrase("OK"),
		  headers(),
		  body(),
		  closeConnection(true)
	{
	}

	// for quick errors
	HttpResponse(int code, const std::string& reason, const std::string& responseBody)
		: statusCode(code),
		  reasonPhrase(reason),
		  headers(),
		  body(responseBody),
		  closeConnection(true)
	{
	}
};

#endif