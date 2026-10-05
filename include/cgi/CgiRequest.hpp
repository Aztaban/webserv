#ifndef CGIREQUEST_HPP
# define CGIREQUEST_HPP

# include "http/HttpRequest.hpp"

# include <string>
# include <vector>

class CgiRequest
{

public:
    std::string                 scriptPath;
    std::string                 interpreterPath;
    std::string                 workingDirectory;
    std::vector<std::string>    env;
    std::string                 body;
	std::string                 bodyFilePath;

    CgiRequest(/* args */);
    ~CgiRequest();

	void swap(CgiRequest& other);
};


#endif