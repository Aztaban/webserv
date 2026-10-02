#include "cgi/CgiJob.hpp"
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <dirent.h>
#include <poll.h>
#include <cstdarg>
#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <string>

static int failPipe = 0, failFcntl = 0, failDup = 0;
static bool failFork = false, failRead = false, failWrite = false;
static int pipes = 0, controls = 0, duplicates = 0, forks = 0;
extern "C" int __real_pipe(int*);
extern "C" ssize_t __real_read(int, void*, size_t);
extern "C" ssize_t __real_write(int, const void*, size_t);
extern "C" ssize_t __wrap_read(int fd, void *buffer, size_t size)
{
    if (failRead) { errno = EIO; return -1; }
    return __real_read(fd, buffer, size);
}
extern "C" ssize_t __wrap_write(int fd, const void *buffer, size_t size)
{
    if (failWrite) { errno = EPIPE; return -1; }
    return __real_write(fd, buffer, size);
}
extern "C" int __real_fcntl(int, int, ...);
extern "C" pid_t __real_fork();
extern "C" int __real_dup2(int, int);
extern "C" int __wrap_pipe(int *fds)
{
    if (++pipes == failPipe) { errno = EMFILE; return -1; }
    return __real_pipe(fds);
}
extern "C" int __wrap_fcntl(int fd, int command, ...)
{
    va_list args;
    va_start(args, command);
    int value = va_arg(args, int);
    va_end(args);
    if (++controls == failFcntl) { errno = EIO; return -1; }
    return __real_fcntl(fd, command, value);
}
extern "C" pid_t __wrap_fork()
{
    ++forks;
    if (failFork) { errno = EAGAIN; return -1; }
    return __real_fork();
}
extern "C" int __wrap_dup2(int from, int to)
{
    if (++duplicates == failDup) { errno = EBADF; return -1; }
    return __real_dup2(from, to);
}
static void require(bool ok, const char *message)
{
    if (!ok) { std::cerr << "[FAIL] " << message << std::endl; std::exit(1); }
}
static int descriptorCount()
{
    DIR *directory = opendir("/proc/self/fd");
    require(directory != NULL, "Linux /proc/self/fd required for descriptor leak test");
    int count = 0;
    while (readdir(directory)) ++count;
    closedir(directory);
    return count;
}
static CgiRequest request()
{
    CgiRequest result;
    result.scriptPath = "./cgi-bin/echo.py";
    result.interpreterPath = "/usr/bin/python3";
    result.workingDirectory = "./cgi-bin";
    result.env.push_back("REQUEST_METHOD=GET");
    result.env.push_back("CONTENT_LENGTH=0");
    return result;
}
static void reset()
{
    failPipe = failFcntl = failDup = 0;
    pipes = controls = duplicates = forks = 0;
    failFork = failRead = failWrite = false;
}
static void failedStart()
{
    int before = descriptorCount();
    CgiJob job;
    try { job.start(request()); }
    catch (...) { require(false, "startup syscall failure escaped as an exception"); }
    require(job.hasFailed() && job.isDone(), "startup failure must finish with an error");
    require(job.getPid() == -1, "startup failure must not leave a child");
    require(job.getStdinFd() == -1 && job.getStdoutFd() == -1, "startup pipe descriptors remain open");
    require(job.buildResponse().statusCode == 500, "startup failure must produce 500");
    require(descriptorCount() == before, "startup leaked a descriptor");
    if (failFcntl) require(forks == 0, "descriptor setup must finish before fork");
}
static void failedChild(CgiRequest input)
{
    int before = descriptorCount();
    CgiJob job;
    job.start(input);
    job.closeStdin();
    for (int i = 0; i < 100 && !job.isDone(); ++i)
    {
        pollfd fd;
        fd.fd = job.getStdoutFd(); fd.events = POLLIN; fd.revents = 0;
        poll(&fd, 1, 20);
        if (fd.revents & POLLIN) job.readFromCgi();
        else if (fd.revents & (POLLHUP | POLLERR | POLLNVAL)) job.closeStdout();
        job.updateProcessState();
    }
    require(job.isDone() && job.hasFailed(), "failed child setup must terminate with an error");
    require(job.getPid() == -1, "failed child must be reaped");
    require(job.buildResponse().statusCode == 500, "child setup failure must produce 500");
    require(descriptorCount() == before, "failed child setup leaked a descriptor");
}
static void failedIo(bool writing)
{
    int before = descriptorCount();
    CgiRequest input = request();
    if (writing)
    {
        input.body = "body";
        input.env[1] = "CONTENT_LENGTH=4";
    }
    CgiJob job;
    job.start(input);
    if (!writing) job.closeStdin();
    pollfd fd;
    fd.fd = writing ? job.getStdinFd() : job.getStdoutFd();
    fd.events = writing ? POLLOUT : POLLIN;
    fd.revents = 0;
    require(poll(&fd, 1, 2000) > 0 && (fd.revents & fd.events), "CGI pipe did not become ready");
    if (writing) { failWrite = true; job.writeToCgi(); failWrite = false; }
    else { failRead = true; job.readFromCgi(); failRead = false; }
    require(job.hasFailed(), "pipe I/O failure must mark the CGI job failed");
    job.closeStdin();
    job.closeStdout();
    job.killProcess();
    for (int i = 0; i < 100 && !job.isDone(); ++i)
    {
        poll(NULL, 0, 20);
        job.updateProcessState();
    }
    require(job.isDone() && job.getPid() == -1, "pipe error cleanup must reap child");
    require(job.buildResponse().statusCode == 500, "pipe error must produce 500");
    require(descriptorCount() == before, "pipe error leaked descriptors");
}
int main()
{
    for (int i = 1; i <= 2; ++i) { reset(); failPipe = i; failedStart(); }
    for (int i = 1; i <= 2; ++i) { reset(); failFcntl = i; failedStart(); }
    reset(); failFork = true; failedStart();
    for (int i = 1; i <= 2; ++i) { reset(); failDup = i; failedChild(request()); }
    reset(); CgiRequest badDirectory = request();
    badDirectory.workingDirectory = "/no/such/cgi-directory"; failedChild(badDirectory);
    reset(); CgiRequest badInterpreter = request();
    badInterpreter.interpreterPath = "/no/such/cgi-interpreter"; failedChild(badInterpreter);
    reset(); CgiRequest badScript = request();
    badScript.scriptPath = "/no/such/cgi-script"; failedChild(badScript);
    reset(); failedIo(false);
    reset(); failedIo(true);
    std::cout << "[PASS] CGI startup and I/O failure tests (12 scenarios)" << std::endl;
}
