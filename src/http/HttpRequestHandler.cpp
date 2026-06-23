#include "http/HttpRequestHandler.hpp"

// ---------------- ORTODOX CANON ----------------
HttpRequestHandler::HttpRequestHandler(const std::string& root)
	: _root(root) {}

HttpRequestHandler::HttpRequestHandler(const HttpRequestHandler& other)
	: _root(other._root) {}

HttpRequestHandler& HttpRequestHandler::operator=(const HttpRequestHandler& other)
{
	if (this != &other)
		_root = other._root;
	return *this;
}

HttpRequestHandler::~HttpRequestHandler() {}

// ---------------- FILE READER ----------------

static std::string readFile(const std::string& path)
{
	std::ifstream file(path.c_str());
	if (!file.is_open())
		return "";

	std::ostringstream ss;
	ss << file.rdbuf();
	return ss.str();
}

// ---------------- DISPATCH ----------------

HttpResponse HttpRequestHandler::handle(const HttpRequest& request)
{
	if (request.method == "GET")
		return _handleGET(request);

	if (request.method == "POST")
		return _handlePOST(request);

	if (request.method == "DELETE")
		return _handleDELETE(request);

	return _error405();
}

// ---------------- GET ----------------

HttpResponse HttpRequestHandler::_handleGET(const HttpRequest& request)
{
	HttpResponse res;

	std::string path = _resolvePath(request.target);
	std::string content = readFile(path);

	if (content.empty())
		return _error404();

	res.statusCode = 200;
	res.reasonPhrase = "OK";
	res.body = content;

	return res;
}

// ---------------- POST ----------------

HttpResponse HttpRequestHandler::_handlePOST(const HttpRequest& request)
{
	HttpResponse res;

	res.statusCode = 200;
	res.reasonPhrase = "OK";
	res.body = "POST received: " + request.body + "\n";

	return res;
}

// ---------------- DELETE ----------------

HttpResponse HttpRequestHandler::_handleDELETE(const HttpRequest& request)
{
	(void)request;

	HttpResponse res;

	res.statusCode = 200;
	res.reasonPhrase = "OK";
	res.body = "DELETE not implemented yet";

	return res;
}

// ---------------- ERRORS ----------------

HttpResponse HttpRequestHandler::_error404()
{
	HttpResponse res;

	res.statusCode = 404;
	res.reasonPhrase = "Not Found";
	res.body = "404 Not Found";

	return res;
}

HttpResponse HttpRequestHandler::_error405()
{
	HttpResponse res;

	res.statusCode = 405;
	res.reasonPhrase = "Method Not Allowed";
	res.body = "405 Method Not Allowed";

	return res;
}

// ---------------- PATH RESOLUTION ----------------

std::string HttpRequestHandler::_resolvePath(const std::string& target)
{
	std::string path = _root;

	if (target == "/")
		return path + "/index.html";

	path += target;

	if (!target.empty() && target[target.size() - 1] == '/')
		path += "index.html";

	return path;
}