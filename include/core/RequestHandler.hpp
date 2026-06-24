#ifndef REQUESTHANDLER_HPP
# define REQUESTHANDLER_HPP

# include "http/HttpRequest.hpp"
# include "http/HttpResponse.hpp"
# include "core/RequestContext.hpp"

# include <string>

class RequestHandler {

public:
    static HttpResponse handle(const HttpRequest& request, const RequestContext& context);
    // Temporary compatibility function.
    // Remove later when ClientConnection passes parsed HttpRequest directly.
    //static std::string handleRawRequest(const std::string &rawRequest);

};

#endif