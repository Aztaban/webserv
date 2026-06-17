#ifndef HTTPRESPONSE_HPP
#define HTTPRESPONSE_HPP

#include <string>
#include <map>

//store informations only, it contains everything needed to send back to browser:
class HttpResponse
{
	public:
		std::map<std::string, std::string> headers;
		std::string body;

		int statusCode;
		std::string reasonPhrase;
};

#endif