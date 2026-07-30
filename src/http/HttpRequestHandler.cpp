#include "http/HttpRequestHandler.hpp"

// ----------------- CONSTRUCTORS ----------------
HttpRequestHandler::HttpRequestHandler(
	const std::string& root,
	const std::string& locationPath,
	const std::string& uploadDirectory,
	const std::string& index,
	const std::vector<std::string>& methods,
	const std::map<int, std::string>& errorPages,
	bool autoindexEnabled,
	size_t maxBodySize) :
	_root(root),
	_locationPath(locationPath),
	_uploadDirectory(uploadDirectory),
	_index(index),
	_allowedMethods(methods),
	_errorPages(errorPages),
	_autoindexEnabled(autoindexEnabled),
	_maxBodySize(maxBodySize)
{}

HttpRequestHandler::HttpRequestHandler(const HttpRequestHandler& other):
	_root(other._root),
	_locationPath(other._locationPath),
	_uploadDirectory(other._uploadDirectory),
	_index(other._index),
	_allowedMethods(other._allowedMethods),
	_errorPages(other._errorPages),
	_autoindexEnabled(other._autoindexEnabled),
	_maxBodySize(other._maxBodySize)
{}

HttpRequestHandler& HttpRequestHandler::operator=(const HttpRequestHandler& other)
{
	if (this != &other)
	{
		_root = other._root;
		_locationPath = other._locationPath;
		_uploadDirectory = other._uploadDirectory;
		_index = other._index;
		_allowedMethods = other._allowedMethods;
		_errorPages = other._errorPages;
		_autoindexEnabled = other._autoindexEnabled;
		_maxBodySize = other._maxBodySize;
	}

	return *this;
}

HttpRequestHandler::~HttpRequestHandler()
{}

// ----------------- HELPERS ----------------
bool HttpRequestHandler::_methodAllowed(
	const std::string& method)
{
	if (_allowedMethods.empty())
		return true;

	for (size_t i = 0; i < _allowedMethods.size(); i++)
	{
		if (_allowedMethods[i] == method)
			return true;
	}
	return false;
}

// Creates error responses using configured error handling.
HttpResponse HttpRequestHandler::_error(int code)
{
	return ErrorResponseFactory::create(code, _root);
}

// ----------------- MAIN HANDLER ----------------
HttpResponse HttpRequestHandler::handle(const HttpRequest& request)
{
	if (!_methodAllowed(request.method))
		return _error(405);

	if (request.method == "GET")
		return _handleGET(request);

	if (request.method == "POST")
		return _handlePOST(request);

	if (request.method == "DELETE")
		return _handleDELETE(request);

	return _error(405);
}

// ----------------- GET ----------------
HttpResponse HttpRequestHandler::_handleGET(const HttpRequest& request)
{
	HttpResponse res;
	std::string target;
	std::string path;
	struct stat st;

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

	if (S_ISDIR(st.st_mode))
	{
		if (!path.empty()
			&& path[path.size() - 1] != '/')
		{
			path += "/";
		}

		std::string indexName = _index;

		if (indexName.empty())
			indexName = "index.html";

		std::string indexPath =
			path + indexName;

		if (FileUtils::exists(indexPath))
		{
			path = indexPath;
		}
		else if (_autoindexEnabled)
		{
			return _generateAutoIndex(
				path,
				request.target);
		}
		else
		{
			return _error(404);
		}
	}

	res.statusCode = 200;
	res.reasonPhrase = "OK";
	res.body = FileUtils::read(path);
	res.headers["Content-Type"] =
		FileUtils::mimeType(path);

	return res;
}

// ----------------- POST ----------------
HttpResponse HttpRequestHandler::_handlePOST(const HttpRequest& request)
{
	HttpResponse res;

	std::string target;

	if (request.path.empty())
		target = request.target;
	else
		target = request.path;

	std::string uploadDir = _uploadDirectory;

	if (uploadDir.empty())
		uploadDir = _root + "/uploads/";

	if (_maxBodySize > 0
		&& request.contentLength > _maxBodySize)
	{
		return _error(413);
	}

	std::string fileName = _getFileName(target);

	if (fileName.empty()
		|| fileName.find('/') != std::string::npos
		|| fileName.find("..") != std::string::npos)
	{
		return _error(400);
	}
	if (request.body.empty())
		return _error(400);

	std::string fullPath = uploadDir;

	if (!fullPath.empty() && fullPath[fullPath.size() - 1] != '/')
		fullPath += "/";

	fullPath += fileName;

	std::ofstream out(fullPath.c_str(), std::ios::binary);

	if (!out.is_open())
		return _error(500);

	out.write(request.body.c_str(), request.body.size());

	out.close();
	res.statusCode = 201;
	res.reasonPhrase = "Created";
	res.body = "File uploaded\n";
	res.headers["Content-Type"] = "text/plain";

	return res;
}

// ----------------- DELETE ----------------
HttpResponse HttpRequestHandler::_handleDELETE(
	const HttpRequest& request)
{
	HttpResponse res;

	std::string target;
	std::string path;
	struct stat st;

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


// ----------------- AUTO INDEX ----------------
HttpResponse HttpRequestHandler::_generateAutoIndex(
	const std::string& dirPath,
	const std::string& uri)
{
	HttpResponse res;
	std::ostringstream html;

	DIR* dir =
		opendir(dirPath.c_str());

	if (!dir)
		return _error(404);

	html << "<html><body><h1>Index of "
		 << uri
		 << "</h1><ul>";

	struct dirent* entry;

	while ((entry = readdir(dir)) != NULL)
	{
		std::string name =
			entry->d_name;

		if (name == "." || name == "..")
			continue;

		html << "<li><a href=\""
			 << name;

		if (entry->d_type == DT_DIR)
			html << "/";

		html << "\">"
			 << name
			 << "</a></li>";
	}

	closedir(dir);

	html << "</ul></body></html>";

	res.statusCode = 200;
	res.reasonPhrase = "OK";
	res.body = html.str();
	res.headers["Content-Type"] = "text/html";

	return res;
}

// ----------------- PATH HELPERS ----------------
std::string HttpRequestHandler::_getFileName(
	const std::string& target)
{
	size_t pos =
		target.find_last_of('/');

	if (pos == std::string::npos)
		return target;

	return target.substr(pos + 1);
}

std::string HttpRequestHandler::_resolvePath(
    const std::string& target)
{
    if (target.empty() || target[0] != '/')
        return "";

    std::string cleanTarget = target;

    size_t q = cleanTarget.find('?');
    if (q != std::string::npos)
        cleanTarget.erase(q);

    if (cleanTarget.find("..") != std::string::npos)
        return "";

    // Remove the matched location prefix
    if (_locationPath != "/"
        && cleanTarget.find(_locationPath) == 0)
    {
        cleanTarget.erase(0, _locationPath.length());

        if (cleanTarget.empty())
            cleanTarget = "/";
    }
    // std::cout << "DEBUG root: " << _root << std::endl;
    // std::cout << "DEBUG relative: " << cleanTarget << std::endl;
    // std::cout << "DEBUG final: " << _root + cleanTarget << std::endl;

    return _root + cleanTarget;
}
