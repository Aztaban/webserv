#include "cgi/CgiJob.hpp"
#include "cgi/CgiOutputParser.hpp"

#include <unistd.h>
#include <limits.h>
#include <iostream>
#include <cstdlib>
#include <cerrno>
#include <fcntl.h>
#include <sys/wait.h>

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
	int inFds[2];
	int outFds[2];

	if (pipe(inFds) == -1)
	{
		_failed = true;
		return ;
	}
	if (pipe(outFds) == -1)
	{
		_failed = true;
		close(inFds[1]);
		close(inFds[0]);
		return ;
	}
	_pid = fork();
	if (_pid == 0) // CHILD
	{
		std::vector<char *> envVector;
		char *argv[3];
		for(size_t i = 0; i < request.env.size(); ++i)
			envVector.push_back(const_cast<char*>(request.env[i].c_str()));
		envVector.push_back(NULL);
		char **envArr = &envVector[0];
		argv[0] = const_cast<char*>(request.interpreterPath.c_str());
		char resolved[PATH_MAX];
		if (realpath(request.scriptPath.c_str(), resolved) == NULL)
		{
			std::cerr << "Nonexistent script path" << std::endl;
			_exit(1);
		}
		argv[1] = resolved;
		argv[2] = NULL;
		close(inFds[1]);
		close(outFds[0]);
		dup2(inFds[0], STDIN_FILENO);
		dup2(outFds[1], STDOUT_FILENO);
		chdir(request.workingDirectory.c_str());
		if (execve(request.interpreterPath.c_str(), argv, envArr) == -1)
		{
			close(inFds[0]);
			close(outFds[1]);
			std::cerr << "Error in cgi execution" << std::endl;
			_exit(1);
		}
	}
	else if (_pid == -1)
	{
		_failed = true;
		close(outFds[0]);
		close(outFds[1]);
		close(inFds[0]);
		close(inFds[1]);
		return;
	}
	close(inFds[0]);
	close(outFds[1]);
	_inputBuffer = request.body;
	_stdinClosed = false;
	_stdoutClosed = false;
	_stdinFd = inFds[1];
	_stdoutFd = outFds[0];
	if (fcntl(_stdinFd, F_SETFL, O_NONBLOCK) == -1)
		throw std::runtime_error("fcntl(O_NONBLOCK) failed");
	if (fcntl(_stdoutFd, F_SETFL, O_NONBLOCK) == -1)
		throw std::runtime_error("fcntl(O_NONBLOCK) failed");
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
	size_t remaining = _inputBuffer.size() - _inputOffset;
	ssize_t n = write(_stdinFd, _inputBuffer.c_str() + _inputOffset, remaining);

	if (n > 0)
	{
		_inputOffset += n;
		if (_inputOffset == _inputBuffer.size())
			closeStdin();
	}
	else if (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK ))
	{
		return ;
	}
	else
	{
		_failed = true;
		closeStdin();
		return ;
	}
}

void	CgiJob::readFromCgi()
{
	char buffer[4096];
	ssize_t loaded = read(_stdoutFd, &buffer, 4095);

	if (loaded >= 0)
	{
		_outputBuffer.append(buffer, loaded);
		if (loaded == 0)
		{
			closeStdout();
			return ;
		}
	}
	else if (loaded == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
		return;
	else
	{
		_failed = true;
		closeStdout();
		return ;
	}
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
		int status = 0;
		waitpid(_pid, &status, 0);
		if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
			_done = true;
		else
		{
			_done = true;
			_failed = true;
		}
		close(_stdoutFd);
		_stdoutFd = -1;
	}
	_stdoutClosed = true;
}

void	CgiJob::killProcess()
{
	if (_pid > 0)
	{
		int status = 0;
		kill(_pid, SIGKILL);
		waitpid(_pid, &status, 0);
	}
	_failed = true;
	_done = true;
}

HttpResponse	CgiJob::buildResponse() const
{
	if (_failed)
		return (HttpResponse(500, "Internal Server Error", "CGI failed"));

	return (CgiOutputParser::parse(_outputBuffer));
}
