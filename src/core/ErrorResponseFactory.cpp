#include "core/ErrorResponseFactory.hpp"
#include "utils/FileUtils.hpp"

#include <sstream>

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

HttpResponse	ErrorResponseFactory::create(int code, const std::map<int, std::string>& errorPages)
{
	HttpResponse	res;
	std::string		body;
	std::map<int, std::string>::const_iterator configured;

	res.statusCode = code;
	res.reasonPhrase = _reasonPhrase(code);
	res.closeConnection = true;

	configured = errorPages.find(code);
	if (configured != errorPages.end() && !configured->second.empty())
		body = FileUtils::read(configured->second);

	if (body.empty())
		body = _fallbackBody(code);

	res.body = body;
	res.headers["Content-Type"] = "text/html";

	return (res);
}

HttpResponse	ErrorResponseFactory::create(int code, const ServerConfig* server)
{
	std::string	root;

	if (server)
		return create(code, server->getErrorpages());
	return create(code, std::map<int, std::string>());
}
