#include "http/HttpRequestHandler.hpp"

// ---------------- ORTODOX CANON ----------------
HttpRequestHandler::HttpRequestHandler(const std::string& root)
	: _root(root), _autoindexEnabled(false) {}

HttpRequestHandler::HttpRequestHandler(const HttpRequestHandler& other)
	: _root(other._root), _autoindexEnabled(other._autoindexEnabled) {}

HttpRequestHandler& HttpRequestHandler::operator=(const HttpRequestHandler& other)
{
	if (this != &other)
	{
		_root = other._root;
		_autoindexEnabled = other._autoindexEnabled;
	}
	return *this;
}

HttpRequestHandler::~HttpRequestHandler() {}

// ---------------- FILE EXIST CHECK -----------
static bool fileExists(const std::string& path)
{
	struct stat s;
	return (stat(path.c_str(), &s) == 0);
}

// ---------------- FILE READER ----------------
static std::string readFile(const std::string& path)
{
	std::ifstream file(path.c_str(), std::ios::binary);
	if (!file.is_open())
		return "";

	std::ostringstream ss;
	ss << file.rdbuf();
	return ss.str();
}

// -------------- MIME TYPES -------------
static std::string getMimeType(const std::string& path)
{
	size_t dot = path.find_last_of('.');
	if (dot == std::string::npos)
		return "application/octet-stream";

	std::string ext = path.substr(dot);

	if (ext == ".html") return "text/html";
	if (ext == ".txt")  return "text/plain";
	if (ext == ".css")  return "text/css";
	if (ext == ".js")   return "application/javascript";
	if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
	if (ext == ".png")  return "image/png";

	return "application/octet-stream";
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

	if (request.target.find("..") != std::string::npos)
		return _error404();

	std::string path = _resolvePath(request.target);

	struct stat st;

	// SINGLE SOURCE OF TRUTH
	if (stat(path.c_str(), &st) != 0)
		return _error404();

	// ---------------- DIRECTORY ----------------
	if (S_ISDIR(st.st_mode))
	{
		if (path[path.size() - 1] != '/')
			path += "/";

		std::string indexPath = path + "index.html";

		if (fileExists(indexPath))
			path = indexPath;

		else if (_autoindexEnabled)
			return _generateAutoIndex(path, request.target);

		else
			return _error404();
	}

	// ---------------- FILE ----------------
	if (!fileExists(path))
		return _error404();

	std::string content = readFile(path);

	res.statusCode = 200;
	res.reasonPhrase = "OK";
	res.body = content;
	res.headers["Content-Type"] = getMimeType(path);

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
	res.body = "DELETE not implemented yet\n";
	return res;
}

// ---------------- ERRORS ----------------
HttpResponse HttpRequestHandler::_error404()
{
	HttpResponse res;

	res.statusCode = 404;
	res.reasonPhrase = "Not Found";

	std::string body = readFile(_root + "/site1/errors/404.html");
	if (body.empty())
		body = "404 Not Found";

	res.body = body;
	res.headers["Content-Type"] = "text/html";

	return res;
}

HttpResponse HttpRequestHandler::_error405()
{
	HttpResponse res;

	res.statusCode = 405;
	res.reasonPhrase = "Method Not Allowed";

	std::string body = readFile(_root + "/site1/errors/405.html");
	if (body.empty())
		body = "405 Method Not Allowed";

	res.body = body;
	res.headers["Content-Type"] = "text/html";

	return res;
}

// ---------------- PATH RESOLUTION ----------------
std::string HttpRequestHandler::_resolvePath(const std::string& target)
{
	std::string path = _root;

	if (target == "/")
		return path + "/index.html";

	return path + target;
}

// ---------------- AUTOINDEX ----------------
HttpResponse HttpRequestHandler::_generateAutoIndex(
	const std::string& dirPath,
	const std::string& uri)
{
	(void)dirPath;

	HttpResponse res;
	res.statusCode = 200;
	res.reasonPhrase = "OK";
	res.headers["Content-Type"] = "text/html";

	res.body =
		"<html><body>"
		"<h1>Index of " + uri + "</h1>"
		"<p>TODO: directory listing</p>"
		"</body></html>";

	return res;
}
