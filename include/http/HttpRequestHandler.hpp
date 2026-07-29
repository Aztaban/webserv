#ifndef HTTPREQUESTHANDLER_HPP
#define HTTPREQUESTHANDLER_HPP

#include <string>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <dirent.h>
#include <vector>
#include <map>
#include <cstdio>

#include "core/ErrorResponseFactory.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"
#include "utils/FileUtils.hpp"

class HttpRequestHandler
{
private:
	std::string _root;
	std::string _locationPath;
	std::string _uploadDirectory;
	std::string _index;

	std::vector<std::string> _allowedMethods;
	std::map<int, std::string> _errorPages;

	bool _autoindexEnabled;
	size_t _maxBodySize;

	HttpResponse _error(int code);
	bool _methodAllowed(const std::string &method);

	HttpResponse _handleGET(const HttpRequest &request);
	HttpResponse _handlePOST(const HttpRequest &request);
	HttpResponse _handleDELETE(const HttpRequest &request);

	HttpResponse _generateAutoIndex(const std::string &dirPath, const std::string &uri);

	std::string _getFileName(const std::string &target);

	std::string _resolvePath(const std::string &target);

public:
	HttpRequestHandler(
			const std::string &root,
			const std::string &locationPath,
			const std::string &uploadDirectory,
			const std::string &index,
			const std::vector<std::string> &methods,
			const std::map<int, std::string> &errorPages,
			bool autoindexEnabled,
			size_t maxBodySize);

	HttpRequestHandler(const HttpRequestHandler &other);
	HttpRequestHandler &operator=(const HttpRequestHandler &other);
	~HttpRequestHandler();

	HttpResponse handle(const HttpRequest &request);
};

#endif
