#ifndef HTTPREQUESTHANDLER_HPP
#define HTTPREQUESTHANDLER_HPP

#include <string>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
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
		std::string _uploadDirectory;
		std::string _errorDirectory;

		bool _autoindexEnabled;
		size_t _maxBodySize;

		// Creates error response.
		HttpResponse _error(int code);

		// HTTP method handlers.
		HttpResponse _handleGET(const HttpRequest& request);
		HttpResponse _handlePOST(const HttpRequest& request);
		HttpResponse _handleDELETE(const HttpRequest& request);

		// Helper functions.
		HttpResponse _generateAutoIndex(
			const std::string& dirPath,
			const std::string& uri);

		std::string _getFileName(
			const std::string& target);

		std::string _resolvePath(
			const std::string& target);

	public:

		HttpRequestHandler(
			const std::string& root,
			const std::string& uploadDirectory,
			const std::string& errorDirectory,
			bool autoindexEnabled,
			size_t maxBodySize);

		HttpRequestHandler(
			const HttpRequestHandler& other);

		HttpRequestHandler& operator=(
			const HttpRequestHandler& other);

		~HttpRequestHandler();

		// Executes request handling.
		HttpResponse handle(
			const HttpRequest& request);
};

#endif