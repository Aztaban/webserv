#include "cgi/CgiJob.hpp"
#include "cgi/CgiOutputParser.hpp"

#include <unistd.h>
#include <iostream>
#include <cstdlib>
#include <cerrno>
#include <sys/wait.h>
#include <fcntl.h>
#include <cstdio>

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

void	CgiJob::start(CgiRequest& request, int highestFd)
{
	const bool bodyFromFile = !request.bodyFilePath.empty();
	if (!bodyFromFile)
		_inputBuffer.swap(request.body);
	std::vector<char *> envVector;
	for (size_t i = 0; i < request.env.size(); ++i)
		envVector.push_back(const_cast<char*>(request.env[i].c_str()));
	envVector.push_back(NULL);

	// The child chdir()s into the script's directory, so the file name alone is enough.
	std::string scriptName = request.scriptPath;
	std::string::size_type slash = scriptName.find_last_of('/');
	if (slash != std::string::npos)
		scriptName = scriptName.substr(slash + 1);

	int inFds[2] = {-1, -1};
	int outFds[2] = {-1, -1};
	int inputFileFd = -1;
	if (bodyFromFile)
	{
		inputFileFd = open(request.bodyFilePath.c_str(), O_RDONLY);
		if (inputFileFd == -1)
		{
			std::remove(request.bodyFilePath.c_str());
			request.bodyFilePath.clear();
			_failed = true;
			_done = true;
			return;
		}
	}
	else if (pipe(inFds) == -1)
	{
		_failStart(inFds, outFds);
		return;
	}
	if (pipe(outFds) == -1)
	{
		if (inputFileFd != -1)
			close(inputFileFd);
		if (bodyFromFile)
		{
			std::remove(request.bodyFilePath.c_str());
			request.bodyFilePath.clear();
		}
		_failStart(inFds, outFds);
		return;
	}
	// Parent ends must be non-blocking: poll() only promises *some* free space,
	// a blocking write() of the whole body could freeze the event loop.
	if ((!bodyFromFile && fcntl(inFds[1], F_SETFL, O_NONBLOCK) == -1)
		|| fcntl(outFds[0], F_SETFL, O_NONBLOCK) == -1)
	{
		if (inputFileFd != -1)
			close(inputFileFd);
		if (bodyFromFile)
		{
			std::remove(request.bodyFilePath.c_str());
			request.bodyFilePath.clear();
		}
		_failStart(inFds, outFds);
		return;
	}

	// Highest descriptor the child may have inherited (server fds + these pipes).
	int lastFd = highestFd;
	for (int i = 0; i < 2; ++i)
	{
		if (inFds[i] > lastFd)
			lastFd = inFds[i];
		if (outFds[i] > lastFd)
			lastFd = outFds[i];
	}
	if (inputFileFd > lastFd)
		lastFd = inputFileFd;

	_pid = fork();
	if (_pid == -1)
	{
		if (inputFileFd != -1)
			close(inputFileFd);
		if (bodyFromFile)
		{
			std::remove(request.bodyFilePath.c_str());
			request.bodyFilePath.clear();
		}
		_failStart(inFds, outFds);
		return;
	}
	if (_pid == 0)
	{
		int childStdin = bodyFromFile ? inputFileFd : inFds[0];
		if (dup2(childStdin, STDIN_FILENO) == -1
			|| dup2(outFds[1], STDOUT_FILENO) == -1)
			std::exit(EXIT_FAILURE);
		// Close everything except stdin/stdout/stderr: server sockets, other CGIs'
		// pipes and the original pipe ends. Replaces close-on-exec (F_SETFD).
		for (int fd = 3; fd <= lastFd; ++fd)
			close(fd);
		if (!request.workingDirectory.empty() && chdir(request.workingDirectory.c_str()) == -1)
			std::exit(EXIT_FAILURE);
		char *argv[3];
		argv[0] = const_cast<char*>(request.interpreterPath.c_str());
		argv[1] = const_cast<char*>(scriptName.c_str());
		argv[2] = NULL;
		execve(request.interpreterPath.c_str(), argv, &envVector[0]);
		std::exit(EXIT_FAILURE);
	}
	if (bodyFromFile)
	{
		close(inputFileFd);
		std::remove(request.bodyFilePath.c_str());
		request.bodyFilePath.clear();
		_stdinClosed = true;
		_stdinFd = -1;
	}
	else
	{
		close(inFds[0]);
		_stdinClosed = false;
		_stdinFd = inFds[1];
	}
	close(outFds[1]);
	_stdoutClosed = false;
	_stdoutFd = outFds[0];
}

void	CgiJob::start(const CgiRequest& request, int highestFd)
{
	// Compatibility path for callers that cannot transfer ownership (not used
	// by the server event loop). The normal server path uses the non-const
	// overload above and swaps the body instead of copying it.
	CgiRequest copy = request;
	start(copy, highestFd);
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
	_inputOffset = 0;
	std::string().swap(_inputBuffer);
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

HttpResponse	CgiJob::buildResponse()
{
	if (_failed)
		return (HttpResponse(500, "Internal Server Error", "CGI failed"));

	return (CgiOutputParser::parse(_outputBuffer));
}
