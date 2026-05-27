#include "core/RequestHandler.hpp"
#include "http/HttpRequestParser.hpp"
#include "http/HttpResponseBuilder.hpp"
#include "routing/Router.hpp"

/**
 * Temp integration point for testing.
 * 
 * Recommended future flow:
 * 
 * 1. HttpRequestparser parses rawRequest into HttpRequest
 * 2. Router matches HttpRequest against config
 * 3. Resource handler serves static/upload/delete/cgi etc.
 * 4. HttpResponseBuilder serializes HttpeResponse into raw response string
 * 
 */
std::string RequestHandler::handleRawRequest(const std::string& rawRequest)
{
    (void)rawRequest;
    return HttpResponseBuilder::buildSimpleResponse(200, "OK", "Temporary request handler\n");
}

