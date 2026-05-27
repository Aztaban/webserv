#ifndef HTTPRESPONSE_HPP
#define HTTPRESPONSE_HPP

#include <string>
#include <map>

class HttpResponse
{

public:
    std::map<std::string, std::string> headers;
    std::string reasonPhrase;
    std::string body;
    int         statusCode;
};

#endif