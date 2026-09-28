#include "cgi/CgiJob.hpp"
#include "cgi/CgiOutputParser.hpp"

#include <unistd.h>
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
	if (_pid > 0)
		killProcess();
}

void	CgiJob::_failStart(int inFds[2], int outFds[2])
{
	for (int i = 0; i < 2; ++i)
	{
		if (inFds[i] != -1)
			close(inFds[i]);
		if (outFds[i] != -1)
			close(outFds[i]);
	}
	_failed = true;
	_done = true;
}

void	CgiJob::start(const CgiRequest& request)
{
	// Allocate buffers before acquiring descriptors or creating a child.
	_inputBuffer = request.body;
	std::vector<char *> envVector;
	for (size_t i = 0; i < request.env.size(); ++i)
		envVector.push_back(const_cast<char*>(request.env[i].c_str()));
	envVector.push_back(NULL);

	std::string scriptName = request.scriptPath;
    std::string::size_type slash = scriptName.find_last_of('/');
    if (slash != std::string::npos)
        scriptName = scriptName.substr(slash + 1);
    int inFds[2] = {-1, -1};
    int outFds[2] = {-1, -1};
    if (pipe(inFds) == -1 || pipe(outFds) == -1)
    {
            _failStart(inFds, outFds);
            return;
    }

	_pid = fork();
	if (_pid == -1)
	{
		_failStart(inFds, outFds);
		return;
	}
	if (_pid == 0)
	{
		close(inFds[1]);
		close(outFds[0]);
		if (dup2(inFds[0], STDIN_FILENO) == -1
			|| dup2(outFds[1], STDOUT_FILENO) == -1)
			_exit(1);
		if (inFds[0] != STDIN_FILENO && inFds[0] != STDOUT_FILENO)
			close(inFds[0]);
		if (outFds[1] != STDIN_FILENO && outFds[1] != STDOUT_FILENO)
			close(outFds[1]);
		if (!request.workingDirectory.empty() && chdir(request.workingDirectory.c_str()) == -1)
			_exit(1);
		char *argv[3];
		argv[0] = const_cast<char*>(request.interpreterPath.c_str());
		argv[1] = const_cast<char*>(scriptName.c_str());
		argv[2] = NULL;
		execve(request.interpreterPath.c_str(), argv, &envVector[0]);
		_exit(1);
	}
	close(inFds[0]);
	close(outFds[1]);
	_stdinClosed = false;
	_stdoutClosed = false;
	_stdinFd = inFds[1];
	_stdoutFd = outFds[0];
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
	// One write per POLLOUT event; nonpositive results fail this CGI job.
	ssize_t n = write(_stdinFd, _inputBuffer.c_str() + _inputOffset, remaining);

	if (n > 0)
	{
		_inputOffset += n;
		if (_inputOffset == _inputBuffer.size())
			closeStdin();
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
	// One read per POLLIN event; zero is EOF and a negative result is failure.
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
		close(_stdoutFd);
		_stdoutFd = -1;
	}
	_stdoutClosed = true;
}

void	CgiJob::updateProcessState()
{
	if (_pid > 0)
	{
		int status = 0;
		pid_t result = waitpid(_pid, &status, WNOHANG);
		if (result == _pid)
		{
			_pid = -1;
			if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
				_failed = true;
		}
		else if (result < 0 && errno != EINTR)
		{
			// errno is inspected only after waitpid, never after pipe/socket I/O.
			_failed = true;
			if (errno == ECHILD)
				_pid = -1;
		}
	}
	// Child exit must not discard bytes still buffered in its output pipe.
	_done = (_pid <= 0 && _stdoutClosed);
}

void	CgiJob::killProcess()
{
	_failed = true;
	if (_pid > 0)
		kill(_pid, SIGKILL);
	updateProcessState();
}

HttpResponse	CgiJob::buildResponse() const
{
	if (_failed)
		return (HttpResponse(500, "Internal Server Error", "CGI failed"));

	return (CgiOutputParser::parse(_outputBuffer));
}
