#ifndef HTTPREQUESTHANDLER_HPP
#define HTTPREQUESTHANDLER_HPP

#include <string>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <iostream>

#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"

class HttpRequestHandler
{
	private:
		std::string _root;
		bool _autoindexEnabled;

		HttpResponse _error404();
		HttpResponse _error405();

		HttpResponse _handleGET(const HttpRequest& request);
		HttpResponse _handlePOST(const HttpRequest& request);
		HttpResponse _handleDELETE(const HttpRequest& request);

		std::string _resolvePath(const std::string& target);

		HttpResponse _generateAutoIndex(const std::string& dirPath, const std::string& uri);

	public:
		HttpRequestHandler(const std::string& root);
		HttpRequestHandler(const HttpRequestHandler& other);
		HttpRequestHandler& operator=(const HttpRequestHandler& other);
		~HttpRequestHandler();

		HttpResponse handle(const HttpRequest& request);
};

#endif