#include "core/RequestHandler.hpp"
#include "core/ErrorResponseFactory.hpp"
#include "core/RequestResult.hpp"
#include "http/HttpRequestHandler.hpp"
#include "routing/Router.hpp"
#include "utils/Logger.hpp"
#include <sys/stat.h>
#include <cstdio>
#include <sstream>

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

bool RequestHandler::_isCgiRequest(const HttpRequest& request, const LocationConfig& location, std::string& outScriptPath, std::string& outInterpreter)
{
	std::string restPath;
	if (request.path.find(location.path, 0) != std::string::npos)
		restPath = request.path.substr(location.path.length());
	std::string path = location.root + restPath;
	if (location.cgiEnabled)
	{
		if (path.find_last_of('.') != std::string::npos)
		{
			std::string extension = path.substr(path.find_last_of('.'));
			if (location.cgiExtensions.find(extension) != location.cgiExtensions.end())
			{
				outScriptPath = path;
				outInterpreter = location.cgiExtensions.find(extension)->second;
				return true;
			}
		}
	}
	return false;
}

CgiRequest RequestHandler::_buildCgiRequest(const HttpRequest& httpRequest, const LocationConfig& location, const RequestContext& requestContext)
{
	CgiRequest cgiRequest;
	std::stringstream ss;

	if (_isCgiRequest(httpRequest, location, cgiRequest.scriptPath, cgiRequest.interpreterPath))
	{
		size_t delimiter = cgiRequest.scriptPath.find_last_of("/");
		if (delimiter != std::string::npos)
			cgiRequest.workingDirectory = cgiRequest.scriptPath.substr(0, delimiter);
		cgiRequest.env.push_back("REQUEST_METHOD=" + httpRequest.method);
		cgiRequest.env.push_back("QUERY_STRING=" + httpRequest.query);
		ss << httpRequest.contentLength;
		cgiRequest.env.push_back("CONTENT_LENGTH=" + ss.str());
		ss.clear();
		std::map<std::string, std::string>::const_iterator it = httpRequest.headers.find("Content-Type");
		if (it != httpRequest.headers.end())
			cgiRequest.env.push_back("CONTENT_TYPE=" + it->second);
		cgiRequest.env.push_back("SCRIPT_NAME=" + cgiRequest.scriptPath);
		cgiRequest.env.push_back("PATH_INFO=" + httpRequest.path);
		cgiRequest.env.push_back("SERVER_PROTOCOL=" + httpRequest.version);
		ss << requestContext.localPort;
		cgiRequest.env.push_back("SERVER_NAME=" + requestContext.localHost + "/" + ss.str());
		cgiRequest.env.push_back("GATEWAY_INTERFACE=CGI/1.1");
		if (httpRequest.hasBody)
			cgiRequest.body = httpRequest.body;
	}
	return cgiRequest;
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

	if (!location->methods.empty())
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
			return RequestResult::fromResponse(ErrorResponseFactory::create(405, effectiveContext));
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
			std::remove(path.c_str());
			return RequestResult::fromResponse(ErrorResponseFactory::create(204, effectiveContext));
		}
		else
			return RequestResult::fromResponse(ErrorResponseFactory::create(403, effectiveContext));
	}

	std::string scriptPath, interpreter;
	if (_isCgiRequest(request, *location, scriptPath, interpreter))
	{
		CgiRequest cgiRequest = _buildCgiRequest(request, *location, effectiveContext);
		return RequestResult::fromCgiRequest(cgiRequest);
	}

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

	HttpRequestHandler handler(effectiveRoot);
	return RequestResult::fromResponse(handler.handle(adjustedRequest));
}
