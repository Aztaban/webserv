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

std::string	ErrorResponseFactory::_errorFileName(int code)
{
	std::ostringstream	fileName;

	fileName << code << ".html";
	return (fileName.str());
}

std::string	ErrorResponseFactory::_selectRootFromContext(
	const RequestContext& context)
{
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

HttpResponse	ErrorResponseFactory::create(int code, const RequestContext& context)
{
	std::string	root;

	root = _selectRootFromContext(context);
	return create(code, root);
}
