#ifndef REQUESTRESULT_HPP
# define REQUESTRESULT_HPP

# include "http/HttpResponse.hpp"
# include "cgi/CgiRequest.hpp"

class RequestResult {

public:
    enum Type {
        RESPONSE_READY,
        CGI_REQUIRED
    };

    Type            type;
    HttpResponse    response;
    CgiRequest      cgiRequest;

    RequestResult();

    static RequestResult    fromResponse(const HttpResponse& response);
    static RequestResult    fromCgiRequest(const CgiRequest& cgiRequest);

};

#endif