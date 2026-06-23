#ifndef LOCATIONCONFIG_HPP
# define LOCATIONCONFIG_HPP

# include <string>
# include <vector>
# include <map>

struct LocationConfig
{
    std::string path;   //  "/", "/upload", "/cgi-bin"
    std::string root;
    std::string index;

    std::vector<std::string>    methods;

    bool        autoindex;

    bool        hasRedirect;
    int         redirectCode;
    std::string redurectUrl;

    bool        uploadEnabled;
    std::string uploadPath;

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
          uploadEnabled(false),
          uploadPath(),
          cgiEnabled(false),
          cgiExtensions()
    {
    }
};

#endif