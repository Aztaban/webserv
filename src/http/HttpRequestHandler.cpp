#include "http/HttpRequestHandler.hpp"

// ============================================================
//  HTTP REQUEST HANDLER 
//  → Handles GET / POST / DELETE + static files + autoindex
// ============================================================


// ---------------- CONSTRUCTORS ----------------

// Initializes handler with document root and autoindex enabled
HttpRequestHandler::HttpRequestHandler(const std::string& root)
	: _root(root), _autoindexEnabled(true) {}

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


// ---------------- FILE UTILITIES ----------------

// Checks if file or directory exists
static bool fileExists(const std::string& path)
{
	struct stat s;
	return (stat(path.c_str(), &s) == 0);
}

// Reads a file into a string (binary safe)
static std::string readFile(const std::string& path)
{
	std::ifstream file(path.c_str(), std::ios::binary);
	if (!file.is_open())
		return "";

	std::ostringstream ss;
	ss << file.rdbuf();
	return ss.str();
}

// Returns MIME type based on file extension
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


// ---------------- MAIN DISPATCH ----------------

// Routes request to correct HTTP method handler
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


// ---------------- GET HANDLER ----------------

// Handles static file serving + directory + autoindex
HttpResponse HttpRequestHandler::_handleGET(const HttpRequest& request)
{
	HttpResponse res;

	// Basic traversal protection
	if (request.target.find("..") != std::string::npos)
		return _error404();

	std::string path = _resolvePath(request.target);

	if (path.empty())
		return _error404();

	struct stat st;

	// Check file/directory existence
	if (stat(path.c_str(), &st) != 0)
		return _error404();

	// Directory handling
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

	// File handling
	if (!fileExists(path))
		return _error404();

	std::string content = readFile(path);

	res.statusCode = 200;
	res.reasonPhrase = "OK";
	res.body = content;
	res.headers["Content-Type"] = getMimeType(path);

	return res;
}


// ---------------- POST HELPERS ----------------

// Extracts filename from URL target
std::string HttpRequestHandler::_getFileName(const std::string& target)
{
	size_t pos = target.find_last_of('/');
	if (pos == std::string::npos)
		return target;
	return target.substr(pos + 1);
}


// ---------------- POST HANDLER ----------------

// Handles file upload into /uploads directory
HttpResponse HttpRequestHandler::_handlePOST(const HttpRequest& request)
{
	HttpResponse res;

	std::string uploadDir = _root + "/site1/uploads/";

	std::string fileName = _getFileName(request.target);

	// Validate filename
	if (fileName.empty() || fileName.find('/') != std::string::npos || fileName.find("..") != std::string::npos)
	{
		res.statusCode = 400;
		res.reasonPhrase = "Bad Request";
		res.body = "Invalid filename";
		return res;
	}

	// Validate body
	if (request.body.empty())
	{
		res.statusCode = 400;
		res.reasonPhrase = "Bad Request";
		res.body = "Empty body";
		return res;
	}

	std::string fullPath = uploadDir + fileName;

	std::ofstream out(fullPath.c_str(), std::ios::binary);
	if (!out.is_open())
	{
		res.statusCode = 500;
		res.reasonPhrase = "Internal Server Error";
		res.body = "Cannot write file";
		return res;
	}

	out.write(request.body.c_str(), request.body.size());
	out.close();

	res.statusCode = 201;
	res.reasonPhrase = "Created";
	res.body = "File uploaded\n";
	res.headers["Content-Type"] = "text/plain";

	return res;
}


// ---------------- DELETE HANDLER ----------------

// Placeholder DELETE method (not implemented yet)
HttpResponse HttpRequestHandler::_handleDELETE(const HttpRequest& request)
{
	(void)request;

	HttpResponse res;
	res.statusCode = 200;
	res.reasonPhrase = "OK";
	res.body = "DELETE not implemented yet\n";
	return res;
}


// ---------------- ERROR HANDLERS ----------------

// Returns custom or default 404 page
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

// Returns custom or default 405 page
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


// ---------------- AUTOINDEX ----------------

// Generates directory listing HTML page
HttpResponse HttpRequestHandler::_generateAutoIndex(
	const std::string& dirPath,
	const std::string& uri)
{
	HttpResponse res;

	res.statusCode = 200;
	res.reasonPhrase = "OK";
	res.headers["Content-Type"] = "text/html";

	DIR* dir = opendir(dirPath.c_str());
	if (!dir)
		return _error404();

	std::ostringstream html;

	html << "<!DOCTYPE html>\n"
	     << "<html>\n"
	     << "<head><title>Index of " << uri << "</title></head>\n"
	     << "<body>\n"
	     << "<h1>Index of " << uri << "</h1>\n"
	     << "<ul>\n";

	struct dirent* entry;

	while ((entry = readdir(dir)) != NULL)
	{
		std::string name = entry->d_name;

		if (name == "." || name == "..")
			continue;

		html << "<li><a href=\"" << name;

		if (entry->d_type == DT_DIR)
			html << "/";

		html << "\">" << name;

		if (entry->d_type == DT_DIR)
			html << "/";

		html << "</a></li>\n";
	}

	closedir(dir);

	html << "</ul>\n</body>\n</html>\n";

	res.body = html.str();
	return res;
}