#ifndef HTTPREQUESTHANDLER_HPP
#define HTTPREQUESTHANDLER_HPP

#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"

class HttpRequestHandler
{
	public:
		HttpRequestHandler();
		HttpRequestHandler(const HttpRequestHandler& other);
		HttpRequestHandler& operator=(const HttpRequestHandler& other);
		~HttpRequestHandler();

		HttpResponse handle(const HttpRequest& request);

	private:
		HttpResponse handleGET(const HttpRequest& request);
		HttpResponse handlePOST(const HttpRequest& request);
		HttpResponse handleDELETE(const HttpRequest& request);

		HttpResponse error404();
		HttpResponse error405();
};

#endif