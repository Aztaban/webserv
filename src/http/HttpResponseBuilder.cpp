#include "http/HttpResponseBuilder.hpp"

#include <sstream>

std::string HttpResponseBuilder::buildSimpleResponse(
    int statusCode,
    const std::string &reasonPhrase,
    const std::string &body)
{
    std::ostringstream  response;

    response << "HTTP/1.1 " << statusCode << " " << reasonPhrase << "\r\n";
    response << "Content-Type: text/plain\r\n";
    response << "Content-Length: " << body.size() << "\r\n";
    response << "Connection: close\r\n";
    response << "\r\n";
    response << body;

    return (response.str());
}