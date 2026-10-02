#include "http/HttpRequestHandler.hpp"
#include "core/ErrorResponseFactory.hpp"
#include <cerrno>

// ----------------- CONSTRUCTORS ----------------

HttpRequestHandler::HttpRequestHandler(const std::string& root, const std::string& index,
	bool autoindexEnabled, const std::string& uploadPath)
	: _root(root), _index(index), _autoindexEnabled(autoindexEnabled),
	  _uploadPath(uploadPath) {}

HttpRequestHandler::HttpRequestHandler(const HttpRequestHandler& other)
	: _root(other._root), _index(other._index), _autoindexEnabled(other._autoindexEnabled),
	  _uploadPath(other._uploadPath) {}

HttpRequestHandler& HttpRequestHandler::operator=(const HttpRequestHandler& other)
{
	if (this != &other)
	{
		_root = other._root;
		_index = other._index;
		_autoindexEnabled = other._autoindexEnabled;
		_uploadPath = other._uploadPath;
	}
	return *this;
}

HttpRequestHandler::~HttpRequestHandler() {}

// ----------------- ERROR WRAPPER --------------------
HttpResponse	HttpRequestHandler::_error(int code)
{
	return (ErrorResponseFactory::create(code, _root));
}

// ----------------- MAIN DISPATCH --------------------

HttpResponse HttpRequestHandler::handle(const HttpRequest& request, const std::string& publicPath)
{
	if (request.method == "GET")
		return _handleGET(request, publicPath);
	if (request.method == "POST")
		return _handlePOST(request);
	if (request.method == "DELETE")
		return _handleDELETE(request);

	return _error(501);
}


// ------------------ GET ----------------------

HttpResponse HttpRequestHandler::_handleGET(const HttpRequest& request, const std::string& publicPath)
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

	path = _resolvePath(target);

	// NOTE: if empty -> invalid route or blocked path
	if (path.empty())
		return _error(404);

	if (stat(path.c_str(), &st) != 0)
		return _error((errno == EACCES || errno == EPERM) ? 403 : 404);

	// Directory requests use the configured index first, then listing policy.
	if (S_ISDIR(st.st_mode))
	{
		if (!path.empty() && path[path.size() - 1] != '/')
			path += "/";

		std::string indexPath = path + _index;

		struct stat indexStatus;
		if (stat(indexPath.c_str(), &indexStatus) == 0 && S_ISREG(indexStatus.st_mode))
			path = indexPath;
		else if (_autoindexEnabled)
			return _generateAutoIndex(path, publicPath.empty() ? target : publicPath);
		else
			return _error(404);
	}

	res.statusCode = 200;
	res.reasonPhrase = "OK";
	int readError = FileUtils::read(path, res.body);
	if (readError != 0)
		return _error(readError);
	res.headers["Content-Type"] = FileUtils::mimeType(path);

	return res;
}


// --------------------- POST -------------------------

HttpResponse HttpRequestHandler::_handlePOST(const HttpRequest& request)
{
	HttpResponse	res;
	std::string		target;
	
	if (request.path.empty())
		target = request.target;
	else
		target = request.path;
	
	if (target.find("..") != std::string::npos)
		return _error(400);
	
	std::string uploadDir = _uploadPath.empty() ? _root + "/uploads" : _uploadPath;
	if (uploadDir[uploadDir.size() - 1] != '/')
		uploadDir += "/";

	std::string fileName = _getFileName(target);

	// SECURITY CHECKS (from mine part:))
	if (fileName.empty()
		|| fileName.find('/') != std::string::npos
		|| fileName.find("..") != std::string::npos)
		return _error(400);

	std::string fullPath = uploadDir + fileName;

	if (!FileUtils::write(fullPath, request.body))
		return _error(500);

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

static std::string escapeListingHtml(const std::string& value)
{
	std::string escaped;
	for (std::size_t i = 0; i < value.size(); ++i)
	{
		if (value[i] == '&') escaped += "&amp;";
		else if (value[i] == '<') escaped += "&lt;";
		else if (value[i] == '>') escaped += "&gt;";
		else if (value[i] == '"') escaped += "&quot;";
		else if (value[i] == '\'') escaped += "&#39;";
		else escaped += value[i];
	}
	return escaped;
}

static std::string encodeUriPath(const std::string& path)
{
	const char *hex = "0123456789ABCDEF";
	std::string encoded;
	for (std::size_t i = 0; i < path.size(); ++i)
	{
		unsigned char c = static_cast<unsigned char>(path[i]);
		if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
			|| (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~' || c == '/')
			encoded += c;
		else
		{
			encoded += '%';
			encoded += hex[c >> 4];
			encoded += hex[c & 15];
		}
	}
	return encoded;
}

HttpResponse HttpRequestHandler::_generateAutoIndex(
	const std::string& dirPath,
	const std::string& uri)
{
	HttpResponse res;
	std::ostringstream html;
	DIR* dir = opendir(dirPath.c_str());
	if (!dir)
		return _error(404);

	std::string base = uri;
	if (base.empty() || base[base.size() - 1] != '/')
		base += "/";
	html << "<html><body><h1>Index of " << escapeListingHtml(base) << "</h1><ul>";

	struct dirent* entry;
	while ((entry = readdir(dir)) != NULL)
	{
		std::string name = entry->d_name;
		if (name == "." || name == "..")
			continue;
		struct stat entryStatus;
		bool isDirectory = (stat((dirPath + name).c_str(), &entryStatus) == 0
			&& S_ISDIR(entryStatus.st_mode));
		std::string suffix = isDirectory ? "/" : "";
		html << "<li><a href=\"" << escapeListingHtml(encodeUriPath(base + name + suffix))
			<< "\">" << escapeListingHtml(name + suffix) << "</a></li>";
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

std::string HttpRequestHandler::_resolvePath(const std::string& target)
{
	if (target.empty() || target[0] != '/')
		return "";

	std::string cleanTarget = target;

	if (cleanTarget.find("..") != std::string::npos)
		return "";

	return _root + cleanTarget;
}