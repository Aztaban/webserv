#ifndef HTTPRESPONSEBUILDER_HPP
# define HTTPRESPONSEBUILDER_HPP

# include <string>

class HttpResponseBuilder {

public:
    static std::string  buildSimpleResponse(int statusCode, const std::string& reasonPhrase, const std::string& body);

};

#endif