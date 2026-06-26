# Webserv Network/Core Notes

This document is only about the **network/core part** of the project:

- `WebServer`
- `Listener`
- `EventLoop`
- `ClientConnection`
- small integration types such as `RequestContext`, `RequestResult`, `CgiRequest`, and `CgiJob`

The goal of this part is **not** to implement the full webserver alone.  
The goal is to provide a clean non-blocking network loop where the parser, router/config, response builder, static file handling, uploads, DELETE, and CGI can be connected by the team.

---

## 1. Core responsibility split

### Your part: network/core

```txt
WebServer
  - owns ServerConfig objects
  - creates grouped listeners
  - registers listeners in EventLoop
  - starts the server

Listener
  - owns one listening socket for one host:port
  - accepts new client fds
  - stores ServerConfig candidates for that listener

EventLoop
  - owns the main poll() loop
  - monitors listener sockets and client sockets
  - accepts clients
  - dispatches readable/writable client events
  - removes closed or timed-out clients
  - later: also polls CGI pipe fds if CGI is non-blocking

ClientConnection
  - owns one connected client fd
  - owns read/write buffers
  - owns client state
  - reads only after poll() reports POLLIN
  - writes only after poll() reports POLLOUT
  - feeds received data to HttpRequestParser
  - passes complete HttpRequest + RequestContext to RequestHandler
  - stores serialized response until fully sent
```

### Partner parts

```txt
HttpRequestParser
  - parses HTTP syntax
  - returns incomplete / complete / error
  - produces HttpRequest
  - validates Content-Length, request line, headers, body rules

RequestHandler / Router / Config
  - selects correct ServerConfig
  - selects matching LocationConfig
  - checks allowed methods
  - decides redirect/static/upload/delete/CGI/error behavior

HttpResponseBuilder
  - converts HttpResponse into raw HTTP response bytes

CGI
  - prepares/starts CGI process
  - exposes pipe fds if implemented non-blocking
  - returns CGI output as HttpResponse or data for response building
```

---

## 2. Important 42 network rules

Keep these rules true in the network/core code:

```txt
recv/read from sockets or pipes only after poll() says readable
send/write to sockets or pipes only after poll() says writable
one poll() loop should monitor all server/client I/O
do not branch on errno after recv/send/read/write
client sockets must be non-blocking
listener sockets must be non-blocking
regular disk files do not need poll()
requests must not hang forever
```

Current design follows this for listener sockets and client sockets.

CGI must follow the same rule later if CGI pipes are integrated into your event loop.

---

## 4. Current integration contract with partners

### From parser/response partner

Need stable parser API:

```cpp
class HttpRequestParser
{
public:
    enum ParseStatus
    {
        PARSE_INCOMPLETE,
        PARSE_COMPLETE,
        PARSE_ERROR
    };

    ParseStatus parse(const std::string& rawRequest);
    const HttpRequest& getRequest() const;
    int getErrorStatusCode() const;
    std::size_t getConsumedBytes() const;
    void reset();
};
```

Need stable response builder API:

```cpp
class HttpResponseBuilder
{
public:
    static std::string build(const HttpResponse& response);
    static std::string buildSimpleResponse(int statusCode,
                                           const std::string& reasonPhrase,
                                           const std::string& body);
};
```

Need parser decisions or clear behavior for:

```txt
invalid request line
invalid HTTP version
invalid headers
Content-Length validation
body completeness
body too large / early Content-Length rejection
chunked request handling plan
```

---

### From config/router partner

Need `ServerConfig` to expose:

```txt
host
port
server names if implemented
root
index
client_max_body_size
error pages
locations
```

Need `LocationConfig` to expose:

```txt
path prefix
allowed methods
root
index
autoindex
redirect
upload enabled/path
CGI extension/interpreter
```

Need router behavior:

```txt
select server candidate from RequestContext
select best matching location
return enough data for RequestHandler to handle request
```

---

### From CGI partner

Need agreement on:

```txt
Will CGI be blocking or non-blocking?
Who creates pipes?
Who sets CGI pipe fds non-blocking?
Who owns the child pid?
Who calls waitpid?
Who kills CGI on timeout?
How does EventLoop receive CGI stdout fd?
How is client disconnect handled while CGI is running?
How is CGI output converted into HttpResponse?
```

Recommended direction:

```txt
RequestHandler returns RequestResult::CGI_REQUIRED
EventLoop starts/owns CgiJob
EventLoop polls CGI stdin/stdout fds
ClientConnection waits in CLIENT_WAITING_CGI
when CGI finishes, EventLoop calls client->prepareResponse(response)
client switches to CLIENT_WRITING
```

---

## 5. New TODO list to finish the network/core part

### TODO A — Replace temporary configs with real ConfigParser output

Status: blocked by config parser partner.

Can finish when:

```txt
ConfigParser::parse(path) returns std::vector<ServerConfig>
ServerConfig has final getters used by WebServer/Listener/RequestContext
```

Network/core work needed:

```txt
main.cpp should call ConfigParser
remove hardcoded 127.0.0.1:8000 / 8001 setup
keep temporary fallback only if team wants a dev mode
```

---

### TODO B — Connect final router/location selection in RequestHandler

Status: blocked by config/router partner.

Can finish when:

```txt
Router or equivalent function can choose ServerConfig + LocationConfig
LocationConfig exposes root/index/methods/upload/cgi/redirect
```

Network/core work needed:

```txt
keep RequestHandler interface stable
ensure RequestContext gives all server candidates to RequestHandler
do not add socket logic into RequestHandler
```

---

### TODO C — Decide final body/header limit ownership

Status: partly done, needs parser/config agreement.

Can finish when:

```txt
parser can detect full body completeness
config exposes client_max_body_size
team decides whether parser or ClientConnection performs early Content-Length > limit check
```

Recommended final split:

```txt
ClientConnection protects raw buffer from unlimited growth
HttpRequestParser validates HTTP body/header syntax
RequestHandler/config decides configured client_max_body_size
```

Practical note:

```txt
Current config-driven body check is acceptable.
Later improvement: reject early if Content-Length > client_max_body_size.
```

---

### TODO D — Add/finish network regression tests

Status: can do now.

Can finish without waiting for partners if temporary test config still works.

Recommended tests:

```txt
server accepts connection
GET / returns HTTP/1.1 response
port 8000 and 8001 both work
missing file returns 404
oversized small body returns 413
connection closes after response
client disconnect before request does not crash server
slow incomplete client times out
many clients can connect without blocking each other
```

Notes:

```txt
For oversized body test, use small limit like 10 and body size 12.
Avoid using 1,100,000 bytes for the basic regression test because TCP reset is possible.
```

---

### TODO E — Prepare EventLoop for CGI fd ownership

Status: blocked by CGI partner for final implementation.

Can finish when:

```txt
CgiJob exposes stdin/stdout fds
CgiJob exposes wantsRead/wantsWrite/isDone/buildResponse
CGI partner decides ownership model
```

Network/core work needed later:

```txt
add CGI fd maps in EventLoop
add _isCgiFd()
add _handleCgiEvent()
add _removeCgiJob()
when CGI finishes, call ClientConnection::prepareResponse()
switch client from CLIENT_WAITING_CGI to CLIENT_WRITING
update poll events
```

Possible future maps:

```cpp
std::map<int, CgiJob*> _cgiByFd;
std::map<int, int>     _clientByCgiFd;
std::map<int, CgiJob*> _cgiByClientFd;
```

Do not implement this fully until partner CGI interface is stable.

---

### TODO F — Add CGI cleanup paths

Status: blocked by CGI partner.

Can finish when:

```txt
CgiJob has real process pid/fds
EventLoop owns or can access CgiJob cleanup methods
```

Need to handle:

```txt
CGI process exits normally
CGI process times out
CGI pipe read/write fails
client disconnects while CGI is running
server removes client while CGI still exists
```

Network/core requirement:

```txt
no leaked pipe fds
no zombie CGI process
no dangling client pointer
```

---

### TODO G — Final subject-safe I/O audit after CGI is added

Status: blocked until CGI is implemented.

Can finish when:

```txt
CGI pipes are wired into EventLoop
all socket and pipe read/write calls are known
```

Checklist:

```txt
client recv only after client POLLIN
client send only after client POLLOUT
listener accept only after listener POLLIN
CGI pipe read only after CGI pipe POLLIN
CGI pipe write only after CGI pipe POLLOUT
no errno branching after read/write/recv/send
no blocking pipe I/O
```

---

### TODO H — Optional keep-alive

Status: optional, do only after mandatory behavior works.

Can finish when:

```txt
parser supports consumed bytes
parser supports reset
response builder can set keep-alive/close correctly
team decides to support keep-alive
```

Needed changes:

```txt
after response sent, reset state to reading instead of closing
preserve leftover bytes after consumed request
reset parser for next request
refresh timeout after each request
handle pipelined requests carefully or explicitly close
```

Recommendation:

```txt
Do not do this before GET/POST/DELETE/upload/CGI are stable.
```

---

## 6. What not to do alone now

Avoid these until partner code is ready:

```txt
do not rewrite RequestHandler to return RequestResult yet unless CGI integration starts
do not fully wire CgiJob into EventLoop without CGI partner interface
do not implement router matching alone if config/router partner owns it
do not start keep-alive now
do not move all parser logic into ClientConnection
```

Useful things you can do now:

```txt
clean comments and @brief comments in headers
remove stale temporary comments
add network tests
document network/core contract
run make re and regression tests before each commit
```

---

## 7. Definition of done for your network/core part

Your part is done when:

```txt
server starts from real parsed config
all listeners are non-blocking
all client sockets are non-blocking
one poll loop handles listeners and clients
read/write happens only after poll readiness
multiple clients can connect at once
slow clients do not block other clients
timed-out clients are cleaned
disconnected clients are cleaned
partial writes work
body-too-large requests produce 413
malformed requests produce an error response
listener context reaches RequestHandler
duplicate host:port configs create one listener with candidates
RequestHandler boundary is clean
CGI event-loop integration works if CGI is non-blocking
CGI cleanup paths do not leak fds/processes
network regression tests pass
```

---

## 8. Short explanation for evaluation

Use this if someone asks how your network/core design works:

> `WebServer` owns the configs and creates listeners. Each `Listener` owns one non-blocking listening socket for a unique host and port. The `EventLoop` owns the single `poll()` loop and dispatches listener and client events. A `ClientConnection` owns one client fd, its buffers, state, timeout, and request context. It only reads after `poll()` reports the fd readable and only writes after `poll()` reports the fd writable. HTTP parsing is delegated to `HttpRequestParser`; request dispatching is delegated to `RequestHandler`; response serialization is delegated to `HttpResponseBuilder`. Later, CGI pipe fds can be added to the same event loop so CGI also remains non-blocking.
