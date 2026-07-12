#include "http/HttpRequestHandler.hpp"
#include "core/ErrorResponseFactory.hpp"

// ----------------- CONSTRUCTORS ----------------

HttpRequestHandler::HttpRequestHandler(
	const std::string& root,
	const std::string& uploadDirectory,
	const std::string& errorDirectory,
	bool autoindexEnabled,
	size_t maxBodySize)
	:
	_root(root),
	_uploadDirectory(uploadDirectory),
	_errorDirectory(errorDirectory),
	_autoindexEnabled(autoindexEnabled),
	_maxBodySize(maxBodySize)
{
}

HttpRequestHandler::HttpRequestHandler(
	const HttpRequestHandler& other)
	:
	_root(other._root),
	_uploadDirectory(other._uploadDirectory),
	_errorDirectory(other._errorDirectory),
	_autoindexEnabled(other._autoindexEnabled),
	_maxBodySize(other._maxBodySize)
{
}

HttpRequestHandler& HttpRequestHandler::operator=(
	const HttpRequestHandler& other)
{
	if (this != &other)
	{
		_root = other._root;
		_uploadDirectory = other._uploadDirectory;
		_errorDirectory = other._errorDirectory;
		_autoindexEnabled = other._autoindexEnabled;
		_maxBodySize = other._maxBodySize;
	}

	return *this;
}

HttpRequestHandler::~HttpRequestHandler()
{
}

// ----------------- ERROR WRAPPER --------------------

HttpResponse HttpRequestHandler::_error(int code)
{
	return (ErrorResponseFactory::create(code, _root));
}


// ----------------- MAIN DISPATCH --------------------

HttpResponse HttpRequestHandler::handle(const HttpRequest& request)
{
	if (request.method == "GET")
		return _handleGET(request);
	if (request.method == "POST")
		return _handlePOST(request);
	if (request.method == "DELETE")
		return _handleDELETE(request);

	return _error(405);
}


// ------------------ GET ----------------------

HttpResponse HttpRequestHandler::_handleGET(const HttpRequest& request)
{
	HttpResponse	res;
	std::string		target;
	std::string		path;
	struct stat		st;
	
	if (request.path.empty())
		target = request.target;
	else
		target = request.path;

	// SECURITY: basic path traversal protection
	if (target.find("..") != std::string::npos)
		return _error(404);

	path = _resolvePath(request.target);

	// NOTE: if empty -> invalid route or blocked path
	if (path.empty())
		return _error(404);

	if (stat(path.c_str(), &st) != 0)
		return _error(404);

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
			return _error(404);
	}

	res.statusCode = 200;
	res.reasonPhrase = "OK";
	res.body = FileUtils::read(path);
	res.headers["Content-Type"] = FileUtils::mimeType(path);

	return res;
}


// --------------------- POST -------------------------

HttpResponse HttpRequestHandler::_handlePOST(
	const HttpRequest& request)
{
	HttpResponse res;

	std::string target;

	if (request.path.empty())
		target = request.target;
	else
		target = request.path;

	/*
	 * TODO LADA:
	 *
	 * Upload directory should come from config.
	 *
	 * Example:
	 *
	 * location /uploads {
	 *      upload_path ./uploads;
	 * }
	 *
	 */

	std::string uploadDir = _uploadDirectory;

	// Temporary fallback to keep tests working
	if (uploadDir.empty())
		uploadDir = _root + "/uploads/";

	if (_maxBodySize > 0
		&& request.contentLength > _maxBodySize)
		return _error(413);

	std::string fileName = _getFileName(target);

	if (fileName.empty()
		|| fileName.find('/') != std::string::npos
		|| fileName.find("..") != std::string::npos)
		return _error(400);

	if (request.body.empty())
		return _error(400);

	std::string fullPath = uploadDir + fileName;

	std::ofstream out(
		fullPath.c_str(),
		std::ios::binary);

	if (!out.is_open())
		return _error(500);

	out.write(
		request.body.c_str(),
		request.body.size());

	out.close();

	res.statusCode = 201;
	res.reasonPhrase = "Created";
	res.body = "File uploaded\n";
	res.headers["Content-Type"] = "text/plain";

	return res;
}

// --------------------- DELETE --------------------

HttpResponse HttpRequestHandler::_handleDELETE(const HttpRequest& request)
{
	HttpResponse	res;
	std::string		target;
	std::string		path;
	struct stat		st;

	if (request.path.empty())
		target = request.target;
	else
		target = request.path;

	if (target.find("..") != std::string::npos)
		return _error(404);

	path = _resolvePath(target);
	if (path.empty())
		return _error(404);

	if (stat(path.c_str(), &st) != 0)
		return _error(404);

	if (!S_ISREG(st.st_mode))
		return _error(403);

	if (std::remove(path.c_str()) != 0)
		return _error(500);

	res.statusCode = 204;
	res.reasonPhrase = "No Content";
	res.body = "";
	res.headers["Content-Type"] = "text/plain";

	return res;
}


// -------------- AUTOINDEX -----------------

HttpResponse HttpRequestHandler::_generateAutoIndex(
	const std::string& dirPath,
	const std::string& uri)
{
	HttpResponse		res;
	std::ostringstream	html;

	DIR* dir = opendir(dirPath.c_str());
	if (!dir)
		return _error(404);


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

std::string HttpRequestHandler::_resolvePath(
	const std::string& target)
{
	if (target.empty() || target[0] != '/')
		return "";

	std::string cleanTarget = target;
	size_t q = cleanTarget.find('?');

	if (q != std::string::npos)
		cleanTarget = cleanTarget.substr(0, q);

	if (cleanTarget.find("..") != std::string::npos)
		return "";

	/*
	 * TODO LADA:
	 *
	 * Remove this function after Router integration.
	 *
	 * Router should decide:
	 *
	 * /        -> site root
	 * /uploads -> upload location
	 * /cgi-bin -> CGI execution
	 *
	 */

	return _root + cleanTarget;
}
