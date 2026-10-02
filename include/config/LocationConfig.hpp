#ifndef LOCATIONCONFIG_HPP
# define LOCATIONCONFIG_HPP

# include <string>
# include <vector>
# include <map>
# include <cstddef>

struct LocationConfig
{
    std::string path;   //  "/", "/upload", "/cgi-bin"
    std::string root;
    std::string index;

    std::vector<std::string>    methods;

    bool        autoindex;

    bool        hasRedirect;
    int         redirectCode;
    std::string redirectUrl;

    bool        uploadEnabled;
    std::string uploadPath;
	bool        acceptBody;
	bool        hasClientMaxBodySize;
	std::size_t clientMaxBodySize;

    bool        cgiEnabled;
    std::map<std::string, std::string>  cgiExtensions;   // example: ".py" -> "/usr/bin/python3"

    LocationConfig()
			: path(),
			root(),
			index(),
			methods(),
			autoindex(false),
			hasRedirect(false),
			redirectCode(0),
			redirectUrl(),
			uploadEnabled(false),
			uploadPath(),
			acceptBody(false),
			hasClientMaxBodySize(false),
			clientMaxBodySize(0),
			cgiEnabled(false),
			cgiExtensions()
	{}
};

#endif