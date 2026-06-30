#ifndef HTTPREQUESTHANDLER_HPP
#define HTTPREQUESTHANDLER_HPP

#include <string>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <iostream>
#include <dirent.h>
#include <vector>
#include <cstdio>

#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"
#include "utils/FileUtils.hpp"

class HttpRequestHandler
{
	private:
		std::string _root;
		bool _autoindexEnabled;

		HttpResponse _buildErrorResponse(
		int statusCode,
		const std::string& reason,
		const std::string& fileName,
		const std::string& fallbackMessage);
		
		HttpResponse _error400();
		HttpResponse _error403();
		HttpResponse _error404();
		HttpResponse _error405();
		HttpResponse _error413();
		HttpResponse _error500();
		HttpResponse _error502();

		HttpResponse _handleGET(const HttpRequest& request);
		HttpResponse _handlePOST(const HttpRequest& request);
		HttpResponse _handleDELETE(const HttpRequest& request);

		std::string _resolvePath(const std::string& target);
		
		HttpResponse _generateAutoIndex(const std::string& dirPath, const std::string& uri);

		std::string _getFileName(const std::string& target);

	public:
		HttpRequestHandler(const std::string& root);
		HttpRequestHandler(const HttpRequestHandler& other);
		HttpRequestHandler& operator=(const HttpRequestHandler& other);
		~HttpRequestHandler();

		HttpResponse handle(const HttpRequest& request);
};

#endif