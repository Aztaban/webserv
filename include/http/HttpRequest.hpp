#ifndef HTTPREQUEST_HPP
# define HTTPREQUEST_HPP

# include <string>
# include <map>
# include <cstddef>

struct HttpRequest
{
	public:
		std::string	method;
		std::string	target;	// Full target from request line, e.g. /path?x=1
		std::string	path;	// Clean path without query, e.g. /path
		std::string	query;	// Query string without '?'
		std::string	version;

		std::map<std::string, std::string>	headers;
		std::string	body;
		std::string	transferEncoding;

		bool		isChunked;
		bool		hasBody;
		std::size_t	contentLength;

		HttpRequest()
			: method(),
			  target(),
			  path(),
			  query(),
			  version(),
			  headers(),
			  body(),
			  transferEncoding(),
			  isChunked(false),
			  hasBody(false),
			  contentLength(0)
		{
		}
};

#endif