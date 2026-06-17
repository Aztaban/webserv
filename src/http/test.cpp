#include <iostream>

#include "http/HttpRequestParser.hpp"
#include "http/HttpResponseBuilder.hpp"
#include "http/HttpRequestHandler.hpp"

int main()
{
    std::string raw =
        "POST /upload HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: 5\r\n"
        "\r\n"
        "holi";

    // 1. Parse request
    HttpRequestParser parser;
    parser.parse(raw);

    HttpRequest req = parser.getRequest();

    // 2. Handle request
    HttpRequestHandler handler;
    HttpResponse res = handler.handle(req);

    // 3. Build raw HTTP response
    std::string output = HttpResponseBuilder::buildSimpleResponse(
        res.statusCode,
        res.reasonPhrase,
        res.body
    );

    // 4. Output result
    std::cout << output << std::endl;
}