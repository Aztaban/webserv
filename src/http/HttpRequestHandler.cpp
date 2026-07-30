#include "http/HttpRequestHandler.hpp"
#include "http/HttpHeaderUtils.hpp"

namespace
{
	static bool hasEncodedPathSeparatorOrDot(const std::string& value)
	{
		const std::string lower = HttpHeaderUtils::lowercase(value);

		return lower.find("%2e") != std::string::npos
			|| lower.find("%2f") != std::string::npos
			|| lower.find("%5c") != std::string::npos;
	}

	static bool normalizeUriPath(
		const std::string& target,
		std::string& normalized)
	{
		std::string path = target;
		std::size_t query = path.find('?');
		bool trailingSlash;

		if (query != std::string::npos)
			path.erase(query);
		if (path.empty() || path[0] != '/' || path.find('\\') != std::string::npos
			|| path.find('\0') != std::string::npos
			|| hasEncodedPathSeparatorOrDot(path))
			return false;

		trailingSlash = path.size() > 1 && path[path.size() - 1] == '/';
		normalized.clear();

		std::size_t begin = 1;
		while (begin <= path.size())
		{
			std::size_t end = path.find('/', begin);
			std::string segment;

			if (end == std::string::npos)
				segment = path.substr(begin);
			else
				segment = path.substr(begin, end - begin);

			if (segment == "..")
				return false;
			if (!segment.empty() && segment != ".")
				normalized += "/" + segment;
			if (end == std::string::npos)
				break;
			begin = end + 1;
		}

		if (normalized.empty())
			normalized = "/";
		else if (trailingSlash)
			normalized += "/";
		return true;
	}

	static bool stripLocationPrefix(
		const std::string& target,
		const std::string& locationPath,
		std::string& relative)
	{
		if (locationPath.empty() || locationPath == "/")
		{
			relative = target;
			return true;
		}
		if (target == locationPath || target == locationPath + "/")
		{
			relative = "/";
			return true;
		}
		if (target.size() > locationPath.size()
			&& target.compare(0, locationPath.size(), locationPath) == 0
			&& target[locationPath.size()] == '/')
		{
			relative = target.substr(locationPath.size());
			return true;
		}
		return false;
	}

	static std::string joinPath(
		const std::string& directory,
		const std::string& relative)
	{
		if (directory.empty())
			return "";

		std::string result = directory;
		while (result.size() > 1 && result[result.size() - 1] == '/')
			result.erase(result.size() - 1);

		std::size_t begin = 0;
		while (begin < relative.size() && relative[begin] == '/')
			begin++;
		if (begin == relative.size())
			return result == "/" ? result : result + "/";
		if (result == "/")
			return result + relative.substr(begin);
		return result + "/" + relative.substr(begin);
	}
}

HttpRequestHandler::HttpRequestHandler(
	const std::string& root,
	const std::string& locationPath,
	const std::string& uploadDirectory,
	const std::string& index,
	const std::vector<std::string>& methods,
	const std::map<int, std::string>& errorPages,
	bool autoindexEnabled,
	bool uploadEnabled,
	std::size_t maxBodySize)
	: _root(root),
	  _locationPath(locationPath),
	  _uploadDirectory(uploadDirectory),
	  _index(index),
	  _allowedMethods(methods),
	  _errorPages(errorPages),
	  _autoindexEnabled(autoindexEnabled),
	  _uploadEnabled(uploadEnabled),
	  _maxBodySize(maxBodySize)
{
}

HttpRequestHandler::HttpRequestHandler(const HttpRequestHandler& other)
	: _root(other._root),
	  _locationPath(other._locationPath),
	  _uploadDirectory(other._uploadDirectory),
	  _index(other._index),
	  _allowedMethods(other._allowedMethods),
	  _errorPages(other._errorPages),
	  _autoindexEnabled(other._autoindexEnabled),
	  _uploadEnabled(other._uploadEnabled),
	  _maxBodySize(other._maxBodySize)
{
}

HttpRequestHandler& HttpRequestHandler::operator=(
	const HttpRequestHandler& other)
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
		_uploadEnabled = other._uploadEnabled;
		_maxBodySize = other._maxBodySize;
	}
	return *this;
}

HttpRequestHandler::~HttpRequestHandler() {}

bool HttpRequestHandler::_methodAllowed(const std::string& method) const
{
	if (_allowedMethods.empty())
		return true;
	for (std::size_t i = 0; i < _allowedMethods.size(); i++)
	{
		if (_allowedMethods[i] == method)
			return true;
	}
	return false;
}

HttpResponse HttpRequestHandler::_error(int code)
{
	return ErrorResponseFactory::create(code, _errorPages);
}

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

HttpResponse HttpRequestHandler::_handleGET(const HttpRequest& request)
{
	HttpResponse response;
	std::string path = _resolvePath(
		request.path.empty() ? request.target : request.path);
	struct stat info;

	if (path.empty() || stat(path.c_str(), &info) != 0)
		return _error(404);

	if (S_ISDIR(info.st_mode))
	{
		if (path[path.size() - 1] != '/')
			path += "/";
		const std::string indexName = _index.empty() ? "index.html" : _index;
		const std::string indexPath = path + indexName;

		if (FileUtils::exists(indexPath))
			path = indexPath;
		else if (_autoindexEnabled)
			return _generateAutoIndex(path, request.path);
		else
			return _error(404);
	}

	response.statusCode = 200;
	response.reasonPhrase = "OK";
	response.body = FileUtils::read(path);
	response.headers["Content-Type"] = FileUtils::mimeType(path);
	return response;
}

HttpResponse HttpRequestHandler::_handlePOST(const HttpRequest& request)
{
	HttpResponse response;
	const std::string target = request.path.empty() ? request.target : request.path;

	if (!_uploadEnabled)
		return _error(405);
	if (_uploadDirectory.empty())
		return _error(500);
	if (_maxBodySize > 0
		&& (request.contentLength > _maxBodySize
			|| request.body.size() > _maxBodySize))
		return _error(413);

	const std::string fileName = _getFileName(target);
	if (fileName.empty())
		return _error(400);
	if (request.body.empty())
		return _error(400);

	const std::string fullPath = joinPath(_uploadDirectory, fileName);
	std::ofstream output(fullPath.c_str(), std::ios::binary);
	if (!output.is_open())
		return _error(500);

	output.write(request.body.c_str(), request.body.size());
	if (!output.good())
		return _error(500);
	output.close();

	response.statusCode = 201;
	response.reasonPhrase = "Created";
	response.body = "File uploaded\n";
	response.headers["Content-Type"] = "text/plain";
	return response;
}

HttpResponse HttpRequestHandler::_handleDELETE(const HttpRequest& request)
{
	HttpResponse response;
	const std::string path = _resolvePath(
		request.path.empty() ? request.target : request.path);
	struct stat info;

	if (path.empty() || stat(path.c_str(), &info) != 0)
		return _error(404);
	if (!S_ISREG(info.st_mode))
		return _error(403);
	if (std::remove(path.c_str()) != 0)
		return _error(500);

	response.statusCode = 204;
	response.reasonPhrase = "No Content";
	response.body.clear();
	response.headers["Content-Type"] = "text/plain";
	return response;
}

HttpResponse HttpRequestHandler::_generateAutoIndex(
	const std::string& dirPath,
	const std::string& uri)
{
	HttpResponse response;
	std::ostringstream html;
	DIR* directory = opendir(dirPath.c_str());

	if (!directory)
		return _error(404);

	html << "<html><body><h1>Index of " << uri << "</h1><ul>";
	struct dirent* entry;
	while ((entry = readdir(directory)) != NULL)
	{
		const std::string name = entry->d_name;
		if (name == "." || name == "..")
			continue;
		html << "<li><a href=\"" << name;
		if (entry->d_type == DT_DIR)
			html << "/";
		html << "\">" << name << "</a></li>";
	}
	closedir(directory);

	html << "</ul></body></html>";
	response.statusCode = 200;
	response.reasonPhrase = "OK";
	response.body = html.str();
	response.headers["Content-Type"] = "text/html";
	return response;
}

std::string HttpRequestHandler::_getFileName(const std::string& target) const
{
	std::string normalized;

	if (!normalizeUriPath(target, normalized) || normalized == "/")
		return "";
	if (normalized[normalized.size() - 1] == '/')
		return "";
	const std::size_t slash = normalized.find_last_of('/');
	const std::string fileName = normalized.substr(slash + 1);
	if (fileName.empty() || fileName == "." || fileName == "..")
		return "";
	return fileName;
}

std::string HttpRequestHandler::_resolvePath(const std::string& target) const
{
	std::string normalized;
	std::string relative;

	if (!normalizeUriPath(target, normalized))
		return "";
	if (!stripLocationPrefix(normalized, _locationPath, relative))
		return "";
	return joinPath(_root, relative);
}
