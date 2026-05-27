#ifndef HTTPREQUEST_HPP
# define HTTPREQUEST_HPP

# include <string>
# include <map>

class HttpRequest
{

public:
    std::map<std::string, std::string> headers;
    std::string method;
    std::string target;
    std::string version;
    std::string body;
};

#endif