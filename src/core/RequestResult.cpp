#include "core/RequestResult.hpp"

RequestResult::RequestResult() : type(RESPONSE_READY), response(), cgiRequest()
{
}

RequestResult	RequestResult::fromResponse(const HttpResponse& response)
{
	RequestResult	result;

	result.type = RESPONSE_READY;
	result.response = response;
	return result;
}

RequestResult	RequestResult::fromCgiRequest(const CgiRequest& cgiRequest)
{
	RequestResult	result;

	result.type = CGI_REQUIRED;
	result.cgiRequest = cgiRequest;
	return result;
}