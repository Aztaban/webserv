#include "cgi/CgiJob.hpp"

#include <unistd.h>

CgiJob::CgiJob()
	: _pid(-1),
	  _stdinFd(-1),
	  _stdoutFd(-1),
	  _inputBuffer(),
	  _inputOffset(0),
	  _outputBuffer(),
	  _stdinClosed(true),
	  _stdoutClosed(true),
	  _done(false),
	  _failed(false),
	  _startTime(std::time(NULL))
{
}

CgiJob::~CgiJob()
{
	closeStdin();
	closeStdout();
}

void	CgiJob::start(const CgiRequest& request)
{
	(void)request;
	/*
		TODO:
		- create pipes
		- fork
		- dup2
		- execve
		- set parent pipe fds non-blocking
		- register pipe fds in EventLoop
	*/
	_failed = true;
	_done = true;
}

int CgiJob::getStdinFd() const
{
	return _stdinFd;
}

int CgiJob::getStdoutFd() const
{
	return _stdoutFd;
}

int CgiJob::getPid() const
{
	return _pid;
}

bool	CgiJob::wantsWrite() const
{
	return !_stdinClosed && _inputOffset < _inputBuffer.size();
}

bool	CgiJob::wantsRead() const
{
	return !_stdoutClosed;
}

bool	CgiJob::isDone() const
{
	return _done;
}

bool	CgiJob::hasFailed() const
{
	return _failed;
}

bool	CgiJob::isTimedOut(std::time_t now, int timeoutSeconds) const
{
	return now - _startTime >= timeoutSeconds;
}

void	CgiJob::writeToCgi()
{
	/*
		TODO:
		Write part of _inputBuffer to _stdinFd.
		Only called by EventLoop after poll says fd is writable.
	*/
}

void	CgiJob::readFromCgi()
{
	/*
		TODO:
		Read from _stdoutFd into _outputBuffer.
		Only called by EventLoop after poll says fd is readable.
	*/
}

void	CgiJob::closeStdin()
{
	if (_stdinFd != -1)
	{
		close(_stdinFd);
		_stdinFd = -1;
	}
	_stdinClosed = true;
}

void	CgiJob::closeStdout()
{
	if (_stdoutFd != -1)
	{
		close(_stdoutFd);
		_stdoutFd = -1;
	}
	_stdoutClosed = true;
}

void	CgiJob::killProcess()
{
	/*
		TODO:
		kill(_pid, SIGKILL) if _pid > 0.
		Do not implement until real fork exists.
	*/
	_failed = true;
	_done = true;
}

HttpResponse	CgiJob::buildResponse() const
{
	if (_failed)
		return (HttpResponse(500, "Internal Server Error", "CGI failed"));

	return (HttpResponse(200, "OK", _outputBuffer));
}
