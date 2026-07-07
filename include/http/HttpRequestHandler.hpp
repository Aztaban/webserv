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
		 * current state:)
		 * -----------------------
		 * Diana handles:
		 * - HTTP method dispatch
		 * - GET/POST/DELETE execution
		 * - static file serving
		 * - autoindex generation
		 * - uploads
		 * - errors
		 *
		 * Future/responsibility:
		 * ---------------------
		 * These values should come from:
		 *      ConfigParser -> ServerConfig -> LocationConfig
		 *
		 * Lada should NOT need to modify request handling logic.
		 * you only need to provide correct configuration values.
		 */

		std::string _root;

		// TODO LADA:
		// Comes from location/root configuration.
		// Example:
		// location /uploads {
		//     root /var/www/site;
		// }
		std::string _uploadDirectory;


		// TODO LADA:
		// Comes from error_page configuration.
		// Example:
		// error_page 404 /errors/404.html;
		std::string _errorDirectory;


		// TODO LADA:
		// Comes from:
		// autoindex on/off;
		bool _autoindexEnabled;


		// TODO LADA:
		// Comes from:
		// client_max_body_size
		size_t _maxBodySize;



		// ---------- ERROR HANDLING ----------
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

		// ---------- HTTP METHODS ----------
		HttpResponse _handleGET(const HttpRequest& request);
		HttpResponse _handlePOST(const HttpRequest& request);
		HttpResponse _handleDELETE(const HttpRequest& request);
		/*
		 * CURRENT:
		 * Basic URI -> filesystem mapping.
		 *
		 * TEMPORARY.
		 *
		 * TODO LADA:
		 * Replace with Router result.
		 *
		 * Expected future flow:
		 *
		 * request.path
		 *       |
		 *       v
		 * Router
		 *       |
		 *       v
		 * LocationConfig
		 *       |
		 *       v
		 * filesystem path
		 */
		std::string _resolvePath(const std::string& target);

		// ---------- HELPERS ----------
		HttpResponse _generateAutoIndex(
			const std::string& dirPath,
			const std::string& uri);

		std::string _getFileName(
			const std::string& target);

	public:
		HttpRequestHandler(
			const std::string& root,
			const std::string& uploadDirectory,
			const std::string& errorDirectory,
			bool autoindexEnabled,
			size_t maxBodySize);
		HttpRequestHandler(const HttpRequestHandler& other);
		HttpRequestHandler& operator=(const HttpRequestHandler& other);
		~HttpRequestHandler();
		HttpResponse handle(const HttpRequest& request);
};


#endif