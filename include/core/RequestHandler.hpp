#ifndef REQUESTHANDLER_HPP
# define REQUESTHANDLER_HPP

# include "http/HttpRequest.hpp"
# include "http/HttpResponse.hpp"
# include "core/RequestContext.hpp"
# include "core/RequestResult.hpp"
# include "cgi/CgiRequest.hpp"

/**
 * @brief Central request dispatch entry point.
 *
 * RequestHandler receives a parsed HttpRequest and runtime RequestContext,
 * then produces an HttpResponse. Later this is where server selection,
 * routing, method checks, redirects, uploads, DELETE, static files, and CGI
 * will be connected.
 *
 * It should not read from or write to sockets.
 */
class RequestHandler {

public:
	/**
	 * @brief Handles one parsed request and returns the response.
	 */
	static RequestResult handle(const HttpRequest& request, const RequestContext& context);
	static bool _isCgiRequest(const HttpRequest&, const LocationConfig&, const std::string& root, std::string& outScriptPath, std::string& outInterpreter);
	static CgiRequest _buildCgiRequest(const HttpRequest& httpRequest, const std::string& scriptPath, const std::string& interpreter, const RequestContext& requestContext);
};

#endif
