#ifndef HTTPRESPONSEBUILDER_HPP
#define HTTPRESPONSEBUILDER_HPP

#include "HttpResponse.hpp"
#include <sstream>
#include <map>
#include <string>

//Converts response object -> raw HTTP string
class HttpResponseBuilder
{
	public:
		static std::string  buildSimpleResponse(const HttpResponse& res);
};

#endif