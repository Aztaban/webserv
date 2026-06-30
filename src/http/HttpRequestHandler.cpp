#include "http/HttpRequestHandler.hpp"

// ----------------- CONSTRUCTORS ----------------

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


// ----------------- MAIN DISPATCH --------------------

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


// ------------------ GET ----------------------

HttpResponse HttpRequestHandler::_handleGET(const HttpRequest& request)
{
	HttpResponse res;

	// SECURITY: basic path traversal protection
	if (request.target.find("..") != std::string::npos)
		return _error404();

	std::string path = _resolvePath(request.target);

	// NOTE: if empty -> invalid route or blocked path
	if (path.empty())
		return _error404();

	struct stat st;
	if (stat(path.c_str(), &st) != 0)
		return _error404();

	// DIRECTORY HANDLING (Diana responsibility)
	if (S_ISDIR(st.st_mode))
	{
		if (!path.empty() && path[path.size() - 1] != '/')
			path += "/";

		std::string indexPath = path + "index.html";

		if (FileUtils::exists(indexPath))
			path = indexPath;
		else if (_autoindexEnabled)
			return _generateAutoIndex(path, request.target);
		else
			return _error404();
	}

	res.statusCode = 200;
	res.reasonPhrase = "OK";
	res.body = FileUtils::read(path);
	res.headers["Content-Type"] = FileUtils::mimeType(path);

	std::cout << "GET path: " << path << std::endl;

	return res;
}


// --------------------- POST -------------------------

HttpResponse HttpRequestHandler::_handlePOST(const HttpRequest& request)
{
	HttpResponse res;

	
	// TEMPORARY DESIGN DECISION (NOT FINAL ARCHITECTURE)
	// Upload directory is hardcoded here, but SHOULD come from:
	//   - config file (Lada, will do it:))
	//   - route/location block
	//   - or Router resolution layer
	
	std::string uploadDir = _root + "/site1/uploads/";

	std::string fileName = _getFileName(request.target);

	// SECURITY CHECKS (from mine part:))
	if (fileName.empty() ||
		fileName.find('/') != std::string::npos ||
		fileName.find("..") != std::string::npos)
		return _error400();

	if (request.body.empty())
		return _error400();

	std::string fullPath = uploadDir + fileName;

	std::ofstream out(fullPath.c_str(), std::ios::binary);
	if (!out.is_open())
		return _error500();

	out.write(request.body.c_str(), request.body.size());

	res.statusCode = 201;
	res.reasonPhrase = "Created";
	res.body = "File uploaded\n";
	res.headers["Content-Type"] = "text/plain";

	return res;
}


// --------------------- DELETE --------------------

HttpResponse HttpRequestHandler::_handleDELETE(const HttpRequest& request)
{
	if (request.target.find("..") != std::string::npos)
		return _error404();

	std::string path = _resolvePath(request.target);
	if (path.empty())
		return _error404();

	struct stat st;
	if (stat(path.c_str(), &st) != 0)
		return _error404();

	if (!S_ISREG(st.st_mode))
		return _error403();

	if (std::remove(path.c_str()) != 0)
		return _error500();

	HttpResponse res;
	res.statusCode = 204;
	res.reasonPhrase = "No Content";

	std::cout << "DELETE path: " << path << std::endl;

	return res;
}


// -------------- AUTOINDEX -----------------

HttpResponse HttpRequestHandler::_generateAutoIndex(
	const std::string& dirPath,
	const std::string& uri)
{
	HttpResponse res;

	DIR* dir = opendir(dirPath.c_str());
	if (!dir)
		return _error404();

	std::ostringstream html;

	html << "<html><body><h1>Index of " << uri << "</h1><ul>";

	struct dirent* entry;
	while ((entry = readdir(dir)) != NULL)
	{
		std::string name = entry->d_name;
		if (name == "." || name == "..")
			continue;

		html << "<li><a href=\"" << name;

		if (entry->d_type == DT_DIR)
			html << "/";

		html << "\">" << name << "</a></li>";
	}

	closedir(dir);

	html << "</ul></body></html>";

	res.statusCode = 200;
	res.reasonPhrase = "OK";
	res.body = html.str();
	res.headers["Content-Type"] = "text/html";

	return res;
}


// ------------------- HELPERS ----------------------

std::string HttpRequestHandler::_getFileName(const std::string& target)
{
	size_t pos = target.find_last_of('/');
	if (pos == std::string::npos)
		return target;
	return target.substr(pos + 1);
}


// ---------------- PATH RESOLUTION (TEMPORARY IMPLEMENTATION) ----------------

/* for now is just = basic URI -> filesystem mapping and has minimal sanitization
the isues: hardcoded routes (/uploads, site1), no config awareness, no location block support
so the final thing should be =
YO Diana = asks Router/Config for resolved path
and Lada = provides mapping rules */

std::string HttpRequestHandler::_resolvePath(const std::string& target)
{
	if (target.empty() || target[0] != '/')
		return "";

	std::string cleanTarget = target;

	// Remove query string (OK = mine part)
	size_t q = cleanTarget.find('?');
	if (q != std::string::npos)
		cleanTarget = cleanTarget.substr(0, q);

	// Basic security check (OK but not sufficient alone)
	if (cleanTarget.find("..") != std::string::npos)
		return "";

	// TEMPORARY ROUTING HACK
	// This bypasses proper config-based routing.
	// Should be removed once Lada implements:
	//   - location blocks
	//   - root directives
	//   - alias rules
	// loking forwardssssss:)

	if (cleanTarget.find("/uploads/") == 0)
		return _root + "/site1" + cleanTarget;

	return _root + cleanTarget;
}