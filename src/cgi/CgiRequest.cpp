#include "cgi/CgiRequest.hpp"

CgiRequest::CgiRequest(/* args */)
	: scriptPath(),
	  interpreterPath(),
	  workingDirectory(),
	  env(),
	  body()
{
}

CgiRequest::~CgiRequest()
{
}