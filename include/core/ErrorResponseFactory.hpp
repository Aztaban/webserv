#ifndef ERRORRESPONSEFACTORY_HPP
# define ERRORRESPONSEFACTORY_HPP

# include "http/HttpResponse.hpp"
# include "config/ServerConfig.hpp"

# include <map>
# include <string>

class ErrorResponseFactory
{
private:
	ErrorResponseFactory();
	ErrorResponseFactory(const ErrorResponseFactory& other);
	ErrorResponseFactory& operator=(const ErrorResponseFactory& other);
	~ErrorResponseFactory();

	static std::string _reasonPhrase(int code);
	static std::string _fallbackBody(int code);

public:
	// Uses an explicit error_page map. Missing or unreadable files fall back to
	// the generated built-in page while preserving the requested status code.
	static HttpResponse create(
		int code,
		const std::map<int, std::string>& errorPages);

	// Convenience overload for early errors that already know the selected
	// virtual server. A NULL server uses the generated built-in page.
	static HttpResponse create(int code, const ServerConfig* server);
};

#endif
