#include "http/HttpResponseBuilder.hpp"
#include <sstream>

std::string HttpResponseBuilder::buildSimpleResponse(const HttpResponse& res)
{
	std::ostringstream response;

	// Status line
	response << "HTTP/1.1 "
	         << res.statusCode << " "
	         << res.reasonPhrase << "\r\n";

	// Headers from response object
	for (std::map<std::string, std::string>::const_iterator it = res.headers.begin();
	     it != res.headers.end();
	     ++it)
	{
		response << it->first << ": " << it->second << "\r\n";
	}

	// Ensure required headers exist
	response << "Content-Length: " << res.body.size() << "\r\n";
	response << "Connection: close\r\n";

	// End of headers
	response << "\r\n";

	// Body
	response << res.body;

	return response.str();
}
