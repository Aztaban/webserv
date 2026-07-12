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
		/*
		* Current responsibility:
		* -----------------------
		* - HTTP method dispatch
		* - GET/POST/DELETE execution
		* - static file serving
		* - autoindex generation
		* - uploads
		*
		* Future:
		* -----------------------
		* Values should come from:
		* ConfigParser -> ServerConfig -> LocationConfig
		*/

		std::string _root;

		// Comes from location/root configuration.
		std::string _uploadDirectory;

		// Comes from error_page configuration.
		std::string _errorDirectory;

		// Comes from:
		// autoindex on/off;
		bool _autoindexEnabled;

		// Comes from:
		// client_max_body_size
		size_t _maxBodySize;


		// ---------- ERROR HANDLING ----------

		HttpResponse _error(int code);


		// ---------- HTTP METHODS ----------

		HttpResponse _handleGET(const HttpRequest& request);
		HttpResponse _handlePOST(const HttpRequest& request);
		HttpResponse _handleDELETE(const HttpRequest& request);


		// ---------- HELPERS ----------

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


		HttpResponse handle(
			const HttpRequest& request);
};

#endif
