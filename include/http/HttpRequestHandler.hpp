#ifndef HTTPREQUESTHANDLER_HPP
# define HTTPREQUESTHANDLER_HPP

# include <cstdio>
# include <dirent.h>
# include <fstream>
# include <map>
# include <sstream>
# include <string>
# include <sys/stat.h>
# include <vector>

# include "core/ErrorResponseFactory.hpp"
# include "http/HttpRequest.hpp"
# include "http/HttpResponse.hpp"
# include "utils/FileUtils.hpp"

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
	bool _uploadEnabled;
	std::size_t _maxBodySize;

	HttpResponse _error(int code);
	bool _methodAllowed(const std::string& method) const;
	HttpResponse _handleGET(const HttpRequest& request);
	HttpResponse _handlePOST(const HttpRequest& request);
	HttpResponse _handleDELETE(const HttpRequest& request);
	HttpResponse _generateAutoIndex(const std::string& dirPath, const std::string& uri);
	std::string _getFileName(const std::string& target) const;
	std::string _resolvePath(const std::string& target) const;

public:
	HttpRequestHandler(
		const std::string& root,
		const std::string& locationPath,
		const std::string& uploadDirectory,
		const std::string& index,
		const std::vector<std::string>& methods,
		const std::map<int, std::string>& errorPages,
		bool autoindexEnabled,
		bool uploadEnabled,
		std::size_t maxBodySize);

	HttpRequestHandler(const HttpRequestHandler& other);
	HttpRequestHandler& operator=(const HttpRequestHandler& other);
	~HttpRequestHandler();

	HttpResponse handle(const HttpRequest& request);
};

#endif
