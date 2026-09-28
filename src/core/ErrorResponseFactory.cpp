#include "core/ErrorResponseFactory.hpp"
#include "utils/FileUtils.hpp"
#include "http/HttpHeaderUtils.hpp"

#include <sstream>
#include <fstream>
#include <sys/stat.h>

ErrorResponseFactory::ErrorResponseFactory() {}

ErrorResponseFactory::ErrorResponseFactory(const ErrorResponseFactory& other)
{
	(void)other;
}

ErrorResponseFactory&	ErrorResponseFactory::operator=(const ErrorResponseFactory& other)
{
	(void)other;
	return *this;
}

ErrorResponseFactory::~ErrorResponseFactory() {}

std::string	ErrorResponseFactory::_reasonPhrase(int code)
{
	if (code == 204)
		return ("No Content");
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
	if (code == 500)
		return ("Internal Server Error");
	if (code == 501)
		return ("Not Implemented");
	if (code == 502)
		return ("Bad Gateway");
	if (code == 504)
		return ("Gateway Timeout");
	if (code == 505)
		return ("HTTP Version Not Supported");
	return ("Internal Server Error");
}

std::string	ErrorResponseFactory::_fallbackBody(int code)
{
	std::ostringstream	body;

	body << "<html><body><h1>"
		 << code
		 << " "
		 << _reasonPhrase(code)
		 << "</h1></body></html>";

	return (body.str());
}

std::string	ErrorResponseFactory::_errorFileName(int code)
{
	std::ostringstream	fileName;

	fileName << code << ".html";
	return (fileName.str());
}

std::string	ErrorResponseFactory::_selectRootFromContext(
	const RequestContext& context)
{
	if (context.selectedServer)
		return context.selectedServer->getRoot();
	if (!context.serverCandidates || context.serverCandidates->empty())
		return ("");

	return ((*context.serverCandidates)[0].getRoot());
}

HttpResponse	ErrorResponseFactory::create(int code, const std::string& root)
{
	HttpResponse	res;
	std::string		body;
	std::string		path;

	res.statusCode = code;
	res.reasonPhrase = _reasonPhrase(code);
	res.closeConnection = true;

	if (!root.empty())
	{
		path = root + "/errors/" + _errorFileName(code);
		body = FileUtils::read(path);
	}

	if (body.empty())
		body = _fallbackBody(code);

	res.body = body;
	res.headers["Content-Type"] = "text/html";

	return (res);
}

bool ErrorResponseFactory::applyCustomPage(HttpResponse& response, const RequestContext& context)
{
	if (response.statusCode < 400 || response.statusCode > 599)
		return false;
	const ServerConfig* server = context.selectedServer;
	if (!server && context.serverCandidates && !context.serverCandidates->empty())
		server = &(*context.serverCandidates)[0];
	if (!server)
		return false;
	std::map<int, std::string>::const_iterator it = server->getErrorpages().find(response.statusCode);
	if (it == server->getErrorpages().end() || it->second.empty())
		return false;

	// An empty regular file is a valid page; a missing file or directory is not.
	struct stat status;
	if (stat(it->second.c_str(), &status) != 0 || !S_ISREG(status.st_mode))
		return false;
	std::ifstream file(it->second.c_str(), std::ios::binary);
	if (!file.is_open())
		return false;
	std::ostringstream body;
	body << file.rdbuf();
	if (file.bad() || body.bad())
		return false;

	response.body = body.str();
	// Keep status and policy headers, but discard metadata for the replaced body.
	for (std::map<std::string, std::string>::iterator header = response.headers.begin();
		header != response.headers.end();)
	{
		std::string name = HttpHeaderUtils::lowercase(header->first);
		if (name == "content-type" || name == "content-length" || name == "content-encoding"
			|| name == "content-range" || name == "transfer-encoding")
			response.headers.erase(header++);
		else
			++header;
	}
	response.headers["Content-Type"] = FileUtils::mimeType(it->second);
	return true;
}

HttpResponse	ErrorResponseFactory::create(int code, const RequestContext& context)
{
	HttpResponse response;
	response.statusCode = code;
	response.reasonPhrase = _reasonPhrase(code);
	response.closeConnection = true;
	if (applyCustomPage(response, context))
		return response;
	return create(code, _selectRootFromContext(context));
}
