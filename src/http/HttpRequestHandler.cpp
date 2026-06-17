#include "http/HttpRequestHandler.hpp"

HttpRequestHandler::HttpRequestHandler() {}

HttpRequestHandler::HttpRequestHandler(const HttpRequestHandler& other)
{
	(void)other;
}

HttpRequestHandler& HttpRequestHandler::operator=(const HttpRequestHandler& other)
{
	(void)other;
	return *this;
}

HttpRequestHandler::~HttpRequestHandler() {}

// 

HttpResponse HttpRequestHandler::handle(const HttpRequest& request)
{
	if (request.method == "GET")
		return handleGET(request);

	if (request.method == "POST")
		return handlePOST(request);

	if (request.method == "DELETE")
		return handleDELETE(request);

	return error405();
}

// GET
HttpResponse HttpRequestHandler::handleGET(const HttpRequest& request)
{
	(void)request;
	HttpResponse res;

	if (request.target == "/")
	{
		res.statusCode = 200;
		res.reasonPhrase = "OK";
		res.body = "Welcome to webserv";
		return res;
	}

	if (request.target == "/hello")
	{
		res.statusCode = 200;
		res.reasonPhrase = "OK";
		res.body = "Hola mundito";
		return res;
	}

	return error404();
}

//POST
HttpResponse HttpRequestHandler::handlePOST(const HttpRequest& request)
{
	(void)request;
	HttpResponse res;

	res.statusCode = 200;
	res.reasonPhrase = "OK";
	res.body = "POST received: " + request.body;

	return res;
}

//DELETE
HttpResponse HttpRequestHandler::handleDELETE(const HttpRequest& request)
{
	(void)request;
	HttpResponse res;

	res.statusCode = 200;
	res.reasonPhrase = "OK";
	res.body = "DELETE not implemented yet";

	return res;
}

//ERRORS
HttpResponse HttpRequestHandler::error404()
{
	HttpResponse res;

	res.statusCode = 404;
	res.reasonPhrase = "Not Found";
	res.body = "404 Not Found";

	return res;
}

HttpResponse HttpRequestHandler::error405()
{
	HttpResponse res;

	res.statusCode = 405;
	res.reasonPhrase = "Method Not Allowed";
	res.body = "405 Method Not Allowed";

	return res;
}