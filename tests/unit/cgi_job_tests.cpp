#include "cgi/CgiJob.hpp"
#include "cgi/CgiRequest.hpp"

#include <poll.h>

#include <cstdlib>
#include <ctime>
#include <iostream>
#include <sstream>
#include <string>

/*
** Webserv Unit Tests
**
** Target:
**     CgiJob
**
** These tests drive a CgiJob the same way EventLoop eventually will:
** poll() on whichever fds the job currently wants, then let the job
** read/write. There is no EventLoop/Router wiring yet, so requests
** are built by hand instead of coming from a matched location.
**
** Fixture script: cgi-bin/echo.py (prints Content-Type, method, name,
** query, and body so both GET and POST paths can be checked).
*/

static void require(bool condition, const std::string& message)
{
	if (!condition)
	{
		std::cerr << "[FAIL] " << message << std::endl;
		std::exit(1);
	}
}

static void driveToCompletion(CgiJob& job, int timeoutSeconds)
{
	const int	maxIterations = 1000;
	int			iterations = 0;

	while (!job.isDone())
	{
		if (job.isTimedOut(std::time(NULL), timeoutSeconds))
		{
			job.killProcess();
			break;
		}
		if (++iterations > maxIterations)
			break;

		struct pollfd	fds[2];
		int				n = 0;

		if (job.wantsWrite())
		{
			fds[n].fd = job.getStdinFd();
			fds[n].events = POLLOUT;
			fds[n].revents = 0;
			++n;
		}
		if (job.wantsRead())
		{
			fds[n].fd = job.getStdoutFd();
			fds[n].events = POLLIN;
			fds[n].revents = 0;
			++n;
		}
		if (n == 0)
			break;

		poll(fds, n, 200);

		for (int i = 0; i < n; ++i)
		{
			if ((fds[i].revents & POLLOUT) && fds[i].fd == job.getStdinFd())
				job.writeToCgi();
			if ((fds[i].revents & POLLIN) && fds[i].fd == job.getStdoutFd())
				job.readFromCgi();
			if (fds[i].revents & (POLLERR | POLLHUP | POLLNVAL))
			{
				if (fds[i].fd == job.getStdoutFd())
					job.closeStdout();
				else
					job.closeStdin();
			}
		}
	}
}

static CgiRequest makeEchoRequest(const std::string& query, const std::string& body)
{
	CgiRequest			request;
	std::ostringstream	contentLength;

	contentLength << body.size();

	request.scriptPath = "./cgi-bin/echo.py";
	request.interpreterPath = "/usr/bin/python3";
	request.workingDirectory = "./cgi-bin";
	request.body = body;

	request.env.push_back(std::string("REQUEST_METHOD=") + (body.empty() ? "GET" : "POST"));
	request.env.push_back("QUERY_STRING=" + query);
	request.env.push_back("GATEWAY_INTERFACE=CGI/1.1");
	request.env.push_back("CONTENT_LENGTH=" + contentLength.str());

	return (request);
}

static void test_get_request_runs_script_successfully(void)
{
	CgiRequest	request = makeEchoRequest("name=webserv", "");
	CgiJob		job;

	job.start(request);
	require(!job.hasFailed(), "start() should succeed for a valid interpreter/script");

	driveToCompletion(job, 5);

	require(job.isDone(), "job should finish for a simple GET script");
	require(!job.hasFailed(), "job should not fail for a valid script");

	HttpResponse response = job.buildResponse();

	require(response.statusCode == 200, "successful CGI run should map to 200");
	require(response.body.find("webserv") != std::string::npos,
		"response body should contain the echoed query param");
}

static void test_post_request_writes_full_body_to_cgi(void)
{
	std::string	body = "hello=world&webserv=yes";
	CgiRequest	request = makeEchoRequest("", body);
	CgiJob		job;

	job.start(request);
	require(!job.hasFailed(), "start() should succeed for a valid interpreter/script");

	driveToCompletion(job, 5);

	require(job.isDone(), "job should finish for a POST script");

	HttpResponse response = job.buildResponse();

	require(response.body.find(body) != std::string::npos,
		"response body should contain the full request body echoed back by the script");
}

static void test_bad_interpreter_path_does_not_hang(void)
{
	CgiRequest	request = makeEchoRequest("name=webserv", "");
	CgiJob		job;

	request.interpreterPath = "/no/such/interpreter";

	job.start(request);
	driveToCompletion(job, 5);

	require(job.isDone(), "job should finish (not hang) even when execve fails in the child");

	HttpResponse response = job.buildResponse();

	require(response.statusCode >= 400,
		"a CGI run that never produced valid output should not report success");
}

int main(void)
{
	test_get_request_runs_script_successfully();
	test_post_request_writes_full_body_to_cgi();
	test_bad_interpreter_path_does_not_hang();

	std::cout << "[PASS] CgiJob unit tests" << std::endl;
	return (0);
}
