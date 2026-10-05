#ifndef HTTPRESPONSEBUILDER_HPP
#define HTTPRESPONSEBUILDER_HPP

# include "http/HttpResponse.hpp"

#include <sstream>
#include <map>
#include <string>
#include <cstddef>

//Converts response object -> raw HTTP string
class HttpResponseBuilder
{
	public:
		static std::string	build(const HttpResponse& response);
		static std::string	buildHeaders(const HttpResponse& response, std::size_t bodySize);

		static std::string	buildSimpleResponse(int statusCode, const std::string& reasonPhrase, const std::string& body);
};

#endif