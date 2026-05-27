#ifndef REQUESTHANDLER_HPP
# define REQUESTHANDLER_HPP

# include <string>

class RequestHandler {

public:
    static std::string handleRawRequest(const std::string &rawRequest);

};

#endif