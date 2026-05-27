#include "http/HttpRequestParser.hpp"

HttpRequestParser::HttpRequestParser(): _errorStatusCode(0) {}

HttpRequestParser::ParseStatus  HttpRequestParser::parse(const std::string& rawRequest)
{
    if (rawRequest.find("\r\n\r\n") == std::string::npos)
        return (PARSE_INCOMPLETE);
    _request.method = "GET";
    _request.target = "/";
    _request.version = "HTTP/1.1";

    return (PARSE_COMPLETE);
}

const HttpRequest&  HttpRequestParser::getRequest() const
{
    return (_request);
}

int HttpRequestParser::getErrorStatusCode() const
{
    return (_errorStatusCode);
}