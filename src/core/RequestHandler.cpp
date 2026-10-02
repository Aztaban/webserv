#include "core/RequestHandler.hpp"
#include "core/ErrorResponseFactory.hpp"
#include "core/RequestResult.hpp"
#include "http/HttpRequestHandler.hpp"
#include "http/HttpHeaderUtils.hpp"
#include "routing/Router.hpp"
#include "utils/Logger.hpp"
#include <sys/stat.h>
#include <cstdio>
#include <sstream>
#include <cctype>

static std::string	selectRootFromContext(const RequestContext& context)
{
	if (context.selectedServer)
		return context.selectedServer->getRoot();
	if (!context.serverCandidates || context.serverCandidates->empty())
		return ("");

	return ((*context.serverCandidates)[0].getRoot());
}

// TEMP: local copy of the reason-phrase table (3xx + 4xx used in this file).
// ErrorResponseFactory::_reasonPhrase covers 4xx/5xx but is private, so it
// can't be reused here. Should eventually move to a shared HttpStatus
// utility used by RequestHandler/HttpRequestHandler/ErrorResponseFactory.
static std::string	reasonPhrase(int code)
{
	if (code == 301)
		return ("Moved Permanently");
	if (code == 302)
		return ("Found");
	if (code == 303)
		return ("See Other");
	if (code == 307)
		return ("Temporary Redirect");
	if (code == 308)
		return ("Permanent Redirect");
	if (code == 400)
		return ("Bad Request");
	if (code == 403)
		return ("Forbidden");
	if (code == 404)
		return ("Not Found");
	if (code == 405)
		return ("Method Not Allowed");
	if (code == 413)
		return ("Payload Too Large");
	if (code == 431)
		return ("Request Header Fields Too Large");
	return ("Moved");
}

bool RequestHandler::_isCgiRequest(const HttpRequest& request, const LocationConfig& location, const std::string& root, std::string& outScriptPath, std::string& outInterpreter)
{
    if (!location.cgiEnabled)
            return false;

    // Part of the URL after the location prefix: /cgi-bin/hello.py -> /hello.py
    std::string restPath;
    if (request.path.compare(0, location.path.size(), location.path) == 0)
            restPath = request.path.substr(location.path.size());
    if (!restPath.empty() && restPath[0] != '/')
            restPath = "/" + restPath;
    std::string path = root + restPath;

    // Extension of the file name only, so "/cgi.d/x" or "x.py/extra" are not treated as CGI.
    std::string::size_type slash = path.find_last_of('/');
    std::string::size_type dot = path.find_last_of('.');
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash))
            return false;
    std::map<std::string, std::string>::const_iterator it = location.cgiExtensions.find(path.substr(dot));
    if (it == location.cgiExtensions.end())
            return false;
    outScriptPath = path;
    outInterpreter = it->second;
    return true;
}

static std::string    numberToString(std::size_t number)
{
      std::ostringstream      out;

      out << number;
      return (out.str());
}

static std::string    cgiHeaderName(const std::string& headerName)
{
      std::string     name = headerName;

      for (std::size_t i = 0; i < name.size(); ++i)
      {
              if (name[i] == '-')
                      name[i] = '_';
              else
                      name[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(name[i])));
      }
      return ("HTTP_" + name);
}

CgiRequest RequestHandler::_buildCgiRequest(const HttpRequest& httpRequest, const std::string& scriptPath, const std::string& interpreter, const RequestContext& requestContext)
{
	CgiRequest cgiRequest;

	cgiRequest.scriptPath = scriptPath;
	cgiRequest.interpreterPath = interpreter;
	std::string::size_type delimiter = scriptPath.find_last_of('/');
	if (delimiter != std::string::npos)
	    cgiRequest.workingDirectory = scriptPath.substr(0, delimiter);
	// Request line
	cgiRequest.env.push_back("GATEWAY_INTERFACE=CGI/1.1");
	cgiRequest.env.push_back("SERVER_PROTOCOL=" + httpRequest.version);
	cgiRequest.env.push_back("REQUEST_METHOD=" + httpRequest.method);
	cgiRequest.env.push_back("REQUEST_URI=" + httpRequest.target);
	cgiRequest.env.push_back("QUERY_STRING=" + httpRequest.query);
	cgiRequest.env.push_back("SCRIPT_NAME=" + httpRequest.path);
	cgiRequest.env.push_back("SCRIPT_FILENAME=" + scriptPath);
	cgiRequest.env.push_back("PATH_INFO=" + httpRequest.path);
	cgiRequest.env.push_back("REDIRECT_STATUS=200");
	// Server
	std::string     serverName = requestContext.localHost;
	if (requestContext.selectedServer)
		serverName = requestContext.selectedServer->getName();
	cgiRequest.env.push_back("SERVER_NAME=" + serverName);
	cgiRequest.env.push_back("SERVER_PORT=" + numberToString(requestContext.localPort));
	// Body
	cgiRequest.env.push_back("CONTENT_LENGTH=" + numberToString(httpRequest.contentLength));
	std::string     contentType;
	if (HttpHeaderUtils::getValue(httpRequest.headers, "Content-Type", contentType))
	    cgiRequest.env.push_back("CONTENT_TYPE=" + contentType);
	if (httpRequest.hasBody)
	    cgiRequest.body = httpRequest.body;
	// Every other request header as HTTP_<NAME>
	for (std::map<std::string, std::string>::const_iterator header = httpRequest.headers.begin();
	    header != httpRequest.headers.end(); ++header)
	{
	    std::string     name = cgiHeaderName(header->first);
	    if (name == "HTTP_CONTENT_TYPE" || name == "HTTP_CONTENT_LENGTH")
	        continue;       // already passed above without the HTTP_ prefix
	    cgiRequest.env.push_back(name + "=" + header->second);
	}
	return (cgiRequest);
}


RequestResult	RequestHandler::handle(const HttpRequest& request, const RequestContext& context)
{
	if (!context.serverCandidates || context.serverCandidates->empty())
	{
		Logger::error("RequestHandler received empty RequestContext");
		return RequestResult::fromResponse(HttpResponse(500, "Internal Server Error", "No server configuration"));
	}

	Router router;
	const ServerConfig* selectedServer = context.selectedServer ? context.selectedServer : router.matchServer(*context.serverCandidates, request);
	if (selectedServer == NULL)
		return RequestResult::fromResponse(ErrorResponseFactory::create(400, context));

	RequestContext effectiveContext = context;
	effectiveContext.selectedServer = selectedServer;
	std::string root = selectRootFromContext(effectiveContext);
	if (root.empty())
	{
		Logger::error("RequestHandler received empty RequestContext");
		return RequestResult::fromResponse(HttpResponse(500, "Internal Server Error", "No server configuration"));
	}

	const ServerConfig& server = *selectedServer;
	const LocationConfig* location = router.matchLocation(request, server);
	if (location == NULL)
		return RequestResult::fromResponse(ErrorResponseFactory::create(404, effectiveContext));

	std::string requestRoot = location->root.empty() ? root : location->root;
	if (location->root.empty())
		requestRoot = root;
	else if (!location->path.empty() && request.path.compare(0, location->path.size(), location->path) == 0)
		requestRoot = location->root;

	if (location->hasRedirect)
	{
		HttpResponse response;
		response.statusCode = location->redirectCode;
		response.reasonPhrase = reasonPhrase(location->redirectCode);
		response.headers["Location"] = location->redirectUrl;
		response.closeConnection = true;
		return RequestResult::fromResponse(response);
	}

	if (!location->methods.empty() || request.method == "HEAD")
	{
		bool allowed = false;
		for (std::size_t i = 0; i < location->methods.size(); ++i)
		{
			if (location->methods[i] == request.method)
			{
				allowed = true;
				break;
			}
		}
		if (!allowed)
		{
			HttpResponse response = ErrorResponseFactory::create(405, effectiveContext);
			std::string allow = location->methods.empty() ? "GET, POST, DELETE" : "";
			for (std::size_t i = 0; i < location->methods.size(); ++i)
			{
				if (i != 0)
					allow += ", ";
				allow += location->methods[i];
			}
			response.headers["Allow"] = allow;
			return RequestResult::fromResponse(response);
		}
	}

	if (request.method == "DELETE")
	{
		struct stat	status;
		std::string restPath;

		if (request.path.find("..") != std::string::npos)
			return RequestResult::fromResponse(ErrorResponseFactory::create(403, effectiveContext));
		if (request.path.find(location->path, 0) != std::string::npos)
			restPath = request.path.substr(location->path.length());

		std::string path = location->root.empty() ? root : location->root;
		if (!restPath.empty() && restPath[0] == '/')
			path += restPath;
		else if (!restPath.empty())
			path += "/" + restPath;
		if (stat(path.c_str(), &status) == -1)
			return RequestResult::fromResponse(ErrorResponseFactory::create(404, effectiveContext));
		else if (S_ISREG(status.st_mode))
		{
			if (std::remove(path.c_str()) != 0)
				return RequestResult::fromResponse(ErrorResponseFactory::create(500, effectiveContext));
			return RequestResult::fromResponse(HttpResponse(204, "No Content", ""));
		}
		else
			return RequestResult::fromResponse(ErrorResponseFactory::create(403, effectiveContext));
	}

	std::string scriptPath, interpreter;
	if (_isCgiRequest(request, *location, requestRoot, scriptPath, interpreter))
	{
		struct stat	status;

		if (request.path.find("..") != std::string::npos)
			return RequestResult::fromResponse(ErrorResponseFactory::create(403, effectiveContext));
		// A CGI mapping is an extension -> executable mapping.  The subject tester
		// deliberately POSTs to a matching .bla path that does not exist on disk;
		// the configured CGI executable must still receive that request.  Only
		// reject the target here when it *does* exist and is not a regular file.
		if (stat(scriptPath.c_str(), &status) == 0 && !S_ISREG(status.st_mode))
			return RequestResult::fromResponse(ErrorResponseFactory::create(403, effectiveContext));
		CgiRequest cgiRequest = _buildCgiRequest(request, scriptPath, interpreter, effectiveContext);
		return RequestResult::fromCgiRequest(cgiRequest);
	}

	// CGI POST is independent of permission to store uploaded files.
	if (request.method == "POST" && !location->uploadEnabled)
		return RequestResult::fromResponse(ErrorResponseFactory::create(403, effectiveContext));

	HttpRequest adjustedRequest = request;
	std::string effectiveRoot = requestRoot;
	if (!location->path.empty())
	{
		bool preserveUploadTarget = (request.method == "POST" && location->uploadEnabled && request.path == location->path);
		if (!preserveUploadTarget)
		{
			std::string relativePath = request.path;
			if (relativePath == location->path)
				relativePath = "/";
			else if (relativePath.compare(0, location->path.size(), location->path) == 0)
				relativePath = relativePath.substr(location->path.size());
			if (relativePath.empty())
				relativePath = "/";
			if (relativePath[0] != '/')
				relativePath = "/" + relativePath;
			adjustedRequest.path = relativePath;
			adjustedRequest.target = relativePath;
		}
	}

	const std::string& effectiveIndex = location->index.empty() ? server.getIndex() : location->index;
	HttpRequestHandler handler(effectiveRoot, effectiveIndex, location->autoindex,
		location->uploadPath);
	return RequestResult::fromResponse(handler.handle(adjustedRequest, request.path));
}
