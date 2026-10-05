#include "cgi/CgiRequest.hpp"

CgiRequest::CgiRequest(/* args */)
	: scriptPath(),
	  interpreterPath(),
	  workingDirectory(),
	  env(),
	  body(),
	  bodyFilePath()
{
}

CgiRequest::~CgiRequest()
{
}

void CgiRequest::swap(CgiRequest& other)
{
	scriptPath.swap(other.scriptPath);
	interpreterPath.swap(other.interpreterPath);
	workingDirectory.swap(other.workingDirectory);
	env.swap(other.env);
	body.swap(other.body);
	bodyFilePath.swap(other.bodyFilePath);
}