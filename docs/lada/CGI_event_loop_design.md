# Webserv CGI Event Loop Design

## Purpose

This document defines how CGI should connect to the `webserv` event loop without breaking the 42 subject rules.

The main rule:

```txt
CGI must not be executed as one blocking RequestHandler call.
CGI must be represented as an event-loop job with pipe file descriptors registered in poll().
```

The server must keep serving other clients while one CGI script is running.

---

## Subject-safe rule

The event loop must drive all I/O that can block:

```txt
listener sockets
client sockets
CGI stdin pipe
CGI stdout pipe
```

Regular disk files are different, but sockets and pipes must not be read or written outside readiness from the single poll/equivalent loop.

---

## High-level CGI lifecycle

```txt
1. Client request is fully parsed.
2. Router / RequestHandler decides that the request needs CGI.
3. A CgiRequest is prepared.
4. EventLoop starts a CgiJob.
5. ClientConnection moves to CLIENT_WAITING_CGI.
6. EventLoop polls CGI pipe fds.
7. Server writes request body to CGI stdin when writable.
8. Server reads CGI stdout when readable.
9. When CGI is done, CGI output becomes HttpResponse.
10. ClientConnection receives the response.
11. EventLoop switches client fd to POLLOUT.
12. ClientConnection sends the response and closes.
```

---

## Important design decision

Current simple flow:

```txt
ClientConnection
  -> RequestHandler::handle()
  -> HttpResponse
  -> HttpResponseBuilder::build()
```

This is fine for static/simple responses.

For CGI, the final flow should become:

```txt
ClientConnection reads/parses request
EventLoop asks RequestHandler what to do
RequestHandler returns either:
  - immediate HttpResponse
  - CgiRequest that must be started by EventLoop
EventLoop owns and drives the CgiJob
```

Do not let `RequestHandler` wait for CGI to finish.

---

## Needed request result type

Later, `RequestHandler` should probably return a wrapper instead of only `HttpResponse`.

Example:

```cpp
class RequestResult
{
public:
    enum Type
    {
        RESPONSE_READY,
        CGI_REQUIRED
    };

    Type        type;
    HttpResponse response;
    CgiRequest  cgiRequest;
};
```

Target interface:

```cpp
static RequestResult RequestHandler::handle(const HttpRequest& request,
                                            const RequestContext& context);
```

For now this is only a design target. Do not rewrite the whole request flow until CGI integration starts.

---

## CgiRequest

`CgiRequest` describes what must be executed.

Suggested file:

```txt
include/cgi/CgiRequest.hpp
```

Suggested fields:

```cpp
class CgiRequest
{
public:
    std::string                 scriptPath;
    std::string                 interpreterPath;
    std::string                 workingDirectory;
    std::vector<std::string>    env;
    std::string                 body;
};
```

Example:

```txt
request path:       /cgi-bin/test.py
scriptPath:         ./cgi-bin/test.py
interpreterPath:    /usr/bin/python3
workingDirectory:   ./cgi-bin
body:               request.body
env:                CGI environment variables
```

The router/config layer should create this object after matching the correct location and CGI extension.

---

## CgiJob

`CgiJob` represents one running CGI process.

Suggested file:

```txt
include/cgi/CgiJob.hpp
```

Suggested interface:

```cpp
class CgiJob
{
private:
    pid_t       _pid;

    int         _stdinFd;
    int         _stdoutFd;

    std::string _inputBuffer;
    std::size_t _inputOffset;

    std::string _outputBuffer;

    bool        _stdinClosed;
    bool        _stdoutClosed;
    bool        _done;
    bool        _failed;

    std::time_t _startTime;

public:
    CgiJob();
    ~CgiJob();

    void start(const CgiRequest& request);

    int   getStdinFd() const;
    int   getStdoutFd() const;
    pid_t getPid() const;

    bool wantsWrite() const;
    bool wantsRead() const;
    bool isDone() const;
    bool hasFailed() const;
    bool isTimedOut(std::time_t now, int timeoutSeconds) const;

    void writeToCgi();
    void readFromCgi();

    void closeStdin();
    void closeStdout();
    void killProcess();

    HttpResponse buildResponse() const;
};
```

Concept:

```txt
stdinFd  = fd where server writes request body into CGI
stdoutFd = fd where server reads CGI output
```

Both fds must be non-blocking and registered in poll when needed.

---

## EventLoop ownership

`EventLoop` should own active CGI jobs because it owns the `poll()` set.

Suggested maps:

```cpp
std::map<int, CgiJob*> _cgiByFd;
std::map<int, int>     _cgiFdToClientFd;
std::map<int, CgiJob*> _cgiByClientFd;
```

Meaning:

```txt
_cgiByFd:
  pipe fd -> CgiJob

_cgiFdToClientFd:
  pipe fd -> client fd

_cgiByClientFd:
  client fd -> active CgiJob
```

This lets the event loop handle a CGI fd and know which client should receive the final response.

---

## ClientConnection state

Add a waiting state later:

```cpp
CLIENT_WAITING_CGI
```

Suggested client states:

```txt
CLIENT_READING_HEADERS
CLIENT_READING_BODY
CLIENT_PROCESSING
CLIENT_WAITING_CGI
CLIENT_WRITING
CLIENT_DONE
CLIENT_ERROR
```

When CGI starts:

```txt
client state = CLIENT_WAITING_CGI
client wantsRead()  = false
client wantsWrite() = false
```

The client socket stays open, but no socket I/O happens until CGI produces a response.

---

## Starting a CGI job

When CGI is needed:

```txt
1. Create pipe for CGI stdin.
2. Create pipe for CGI stdout.
3. fork().
4. Child:
   - dup2(stdin read end, STDIN_FILENO)
   - dup2(stdout write end, STDOUT_FILENO)
   - close unused fds
   - chdir(workingDirectory)
   - execve(interpreter/script)
5. Parent:
   - close child-side pipe ends
   - set parent-side pipe fds non-blocking
   - register CGI stdout fd with POLLIN
   - register CGI stdin fd with POLLOUT if body exists
   - if no body exists, close CGI stdin immediately
```

Only CGI may use `fork()`.

---

## EventLoop CGI dispatch

`EventLoop::run()` should later dispatch CGI fds:

```cpp
if (_isListenerFd(fd))
    _handleListenerEvent(fd);
else if (_isClientFd(fd))
    _handleClientEvent(fd, revents);
else if (_isCgiFd(fd))
    _handleCgiEvent(fd, revents);
```

Suggested helper:

```cpp
bool EventLoop::_isCgiFd(int fd) const
{
    return (_cgiByFd.find(fd) != _cgiByFd.end());
}
```

Suggested CGI event handler:

```cpp
void EventLoop::_handleCgiEvent(int fd, short revents)
{
    CgiJob* job = _cgiByFd[fd];
    int clientFd = _cgiFdToClientFd[fd];

    if (revents & (POLLERR | POLLHUP | POLLNVAL))
    {
        _finishCgiForClient(clientFd);
        return;
    }

    if ((revents & POLLOUT) && fd == job->getStdinFd())
        job->writeToCgi();

    if ((revents & POLLIN) && fd == job->getStdoutFd())
        job->readFromCgi();

    if (job->isDone())
        _finishCgiForClient(clientFd);
}
```

---

## Finishing a CGI job

When CGI finishes:

```txt
1. Parse CGI output into HttpResponse.
2. Give response to ClientConnection.
3. Remove CGI fds from poll.
4. Close CGI pipe fds.
5. waitpid(pid, ..., WNOHANG).
6. Delete CgiJob.
7. Switch client to CLIENT_WRITING.
8. Update client poll events to POLLOUT.
```

Possible method:

```cpp
void EventLoop::_finishCgiForClient(int clientFd);
```

`ClientConnection` will need a method like:

```cpp
void ClientConnection::prepareResponse(const HttpResponse& response);
```

which serializes or stores the response and changes state to `CLIENT_WRITING`.

---

## Avoid CGI deadlocks

Bad design:

```txt
write full request body to CGI
then read full CGI output
```

This can deadlock if the pipe buffers fill.

Correct design:

```txt
poll CGI stdin for write readiness
poll CGI stdout for read readiness
write some input when possible
read output whenever available
```

The event loop must be able to write to CGI and read from CGI independently.

---

## CGI timeout

CGI jobs need a timeout.

Example:

```cpp
static const int CGI_TIMEOUT_SECONDS = 5;
```

Timeout behavior:

```txt
if CGI exceeds timeout:
  kill process
  cleanup pipes
  prepare error response for client
```

Suggested statuses:

```txt
504 Gateway Timeout      CGI took too long
502 Bad Gateway          CGI output was malformed
500 Internal Server Error fork/pipe/exec setup failed
```

---

## CGI output parsing

CGI output may look like:

```txt
Content-Type: text/html

<html>Hello</html>
```

or:

```txt
Status: 201 Created
Content-Type: text/plain

Created
```

Suggested helper:

```cpp
class CgiOutputParser
{
public:
    static HttpResponse parse(const std::string& rawOutput);
};
```

Basic rules:

```txt
If Status header exists:
  use it as HTTP status

If Content-Type exists:
  preserve it

If no Content-Length exists:
  response builder can add it from body size

If CGI output is malformed:
  return 502 Bad Gateway
```

For early testing, raw CGI output can temporarily be treated as body, but final behavior should parse CGI headers.

---

## What not to do

Do not implement CGI like this:

```cpp
std::string CgiHandler::execute(...)
{
    fork();
    write(fullBody);
    read(fullOutput);
    waitpid(...);
    return output;
}
```

This blocks the whole server and can deadlock.

---

## Team agreement

Before implementation, agree on these interfaces:

```cpp
class CgiRequest;
class CgiJob;
class CgiOutputParser;
class RequestResult;
```

Also agree on this responsibility split:

```txt
Router / RequestHandler:
  decides whether CGI is needed
  creates CgiRequest

CgiJob:
  owns CGI process and pipe state

EventLoop:
  registers CGI fds in poll
  drives write/read events
  connects CGI completion back to client

ClientConnection:
  waits while CGI is running
  sends final response when EventLoop gives it one
```
