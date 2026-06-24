#ifndef HTTPRESPONSEBUILDER_HPP
#define HTTPRESPONSEBUILDER_HPP

# include "http/HttpResponse.hpp"

# include <string>

//Converts response object -> raw HTTP string
class HttpResponseBuilder
{
	public:
		static std::string	build(const HttpResponse& response);

		static std::string	buildSimpleResponse(int statusCode, const std::string& reasonPhrase, const std::string& body);
};

#endif