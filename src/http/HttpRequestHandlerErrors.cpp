#include "http/HttpRequestHandler.hpp"

// ----------------- ERROR BUILDER -----------------

HttpResponse HttpRequestHandler::_buildErrorResponse(
	int statusCode,
	const std::string& reason,
	const std::string& fileName,
	const std::string& fallback)
{
	HttpResponse res;

	res.statusCode = statusCode;
	res.reasonPhrase = reason;

	std::string path = _root + "/site1/errors/" + fileName;
	std::string body = FileUtils::read(path);

	if (body.empty())
		body = fallback;

	res.body = body;
	res.headers["Content-Type"] = "text/html";

	return res;
}

// ---------------- ERRORS ---------------

HttpResponse HttpRequestHandler::_error400()
{
	return _buildErrorResponse(400, "Bad Request", "400.html", "400 Bad Request");
}

HttpResponse HttpRequestHandler::_error403()
{
	return _buildErrorResponse(403, "Forbidden", "403.html", "403 Forbidden");
}

HttpResponse HttpRequestHandler::_error404()
{
	return _buildErrorResponse(404, "Not Found", "404.html", "404 Not Found");
}

HttpResponse HttpRequestHandler::_error405()
{
	return _buildErrorResponse(405, "Method Not Allowed", "405.html", "405 Method Not Allowed");
}

HttpResponse HttpRequestHandler::_error413()
{
	return _buildErrorResponse(413, "Payload Too Large", "413.html", "413 Payload Too Large");
}

HttpResponse HttpRequestHandler::_error500()
{
	return _buildErrorResponse(500, "Internal Server Error", "500.html", "500 Internal Server Error");
}

HttpResponse HttpRequestHandler::_error502()
{
	return _buildErrorResponse(502, "Bad Gateway", "502.html", "502 Bad Gateway");
}