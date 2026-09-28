#ifndef ERRORRESPONSEFACTORY_HPP
# define ERRORRESPONSEFACTORY_HPP

# include "http/HttpResponse.hpp"
# include "core/RequestContext.hpp"

# include <string>

class ErrorResponseFactory
{
private:
	ErrorResponseFactory();
	ErrorResponseFactory(const ErrorResponseFactory& other);
	ErrorResponseFactory&	operator=(const ErrorResponseFactory& other);
	~ErrorResponseFactory();

	static std::string	_reasonPhrase(int code);
	static std::string	_fallbackBody(int code);
	static std::string	_errorFileName(int code);
	static std::string	_selectRootFromContext(const RequestContext& context);

public:
	static HttpResponse	create(int code, const std::string& root);
	static HttpResponse	create(int code, const RequestContext& context);
	static bool	applyCustomPage(HttpResponse& response, const RequestContext& context);

};

#endif