#ifndef CGIOUTPUTPARSER
# define CGIOUTPUTPARSER

# include "http/HttpResponse.hpp"

# include <string>

class CgiOutputParser {

public:
    static HttpResponse parse(const std::string& rawOutput);

};

#endif