# Webserv Network/Core Notes and TODO

This note is focused on your part of the project: `WebServer`, `Listener`, `EventLoop`, and `ClientConnection`, plus the integration boundary with the HTTP parser, router/config, response builder, and CGI.

The goal is not to turn your network layer into the whole webserver. The goal is to finish the network/core loop so your partners can plug their parsing, routing, config, static file, upload, DELETE, response, and CGI parts into it cleanly.

---

## 1. Most important answer: should parsing be inside `ClientConnection`?

### Short answer

No: **HTTP parsing logic should not live inside `ClientConnection`.**

But yes: **`ClientConnection` may own/use an `HttpRequestParser` object**, because parsing is incremental and the parser state belongs to one client connection.

That distinction is important.

### Correct responsibility split

```txt
ClientConnection
  - owns client fd
  - owns read buffer
  - owns write buffer
  - owns client state
  - receives bytes from socket after poll() says readable
  - sends bytes to socket after poll() says writable
  - asks HttpRequestParser whether the request is incomplete, complete, or invalid
  - switches state based on parser result

HttpRequestParser
  - understands HTTP syntax
  - parses request line
  - parses headers
  - determines body requirements
  - handles Content-Length
  - handles chunked body if your team supports/needs it, especially before CGI
  - validates malformed requests
  - enforces request/header/body limits or receives those limits from config
  - produces HttpRequest or an error status
```

So the temporary logic in `ClientConnection::processReadBuffer()` is acceptable as a development bridge, but later it should become thinner. `ClientConnection` should not manually decide HTTP details like header end, body length, malformed headers, method validity, etc.

Final mental model:

```txt
poll says client fd is readable
  -> ClientConnection::readFromSocket()
      -> append bytes to buffer or feed parser
      -> parser says INCOMPLETE / COMPLETE / ERROR
      -> if COMPLETE: pass parsed HttpRequest to RequestHandler/Router
      -> if ERROR: prepare error HttpResponse
      -> switch to WRITING when response is ready
```

---

## 2. 42 subject rules that affect your network/core design

Keep these rules visible while changing the network loop:

- The project must compile as C++98 with `-Wall -Wextra -Werror`.
- No Boost or external libraries.
- The server must remain non-blocking.
- Use only one `poll()` or equivalent mechanism for all I/O between clients and server, including listening sockets.
- `poll()` must monitor reading and writing.
- Do not call `read`, `recv`, `write`, or `send` on sockets/pipes unless readiness was reported by `poll()` or equivalent.
- Do not check `errno` to adjust behavior after `read`, `recv`, `write`, or `send`.
- Regular disk files are exempt from poll readiness.
- A request must never hang indefinitely.
- You need GET, POST, DELETE, static files, uploads, default error pages, multiple ports, and CGI.
- You cannot `fork()` except for CGI.
- For CGI pipes/FIFOs, the same non-blocking + poll rule applies.

Practical consequence for your code:

```txt
Good:
  poll() reports POLLIN
  -> recv()
  -> if bytes > 0: append
  -> if bytes <= 0: close/finish client without errno-based branching

Good:
  poll() reports POLLOUT
  -> send()
  -> if bytes > 0: erase sent bytes
  -> if bytes <= 0: close/finish client without errno-based branching

Avoid:
  recv()
  if errno == EAGAIN ...   // subject-risky after recv/read

Avoid:
  send()
  if errno == EWOULDBLOCK ... // subject-risky after send/write
```

Your current `bytes <= 0 -> CLIENT_DONE` approach is closer to the subject rule than the usual non-blocking socket tutorial approach that checks `errno` after every `recv()`/`send()`.

---

## 3. Current project state: what is good already

### `main.cpp`

Current status: good temporary bootstrap.

It already supports the required program shape:

```bash
./webserv [configuration file]
```

But config parsing is not plugged in yet. The hardcoded configs are temporary:

```cpp
std::vector<ServerConfig> configs;
configs.push_back(ServerConfig("127.0.0.1", 8000));
configs.push_back(ServerConfig("127.0.0.1", 8001));
```

Later this should become:

```cpp
ConfigParser parser(configPath);
std::vector<ServerConfig> configs = parser.parse();
```

Dependency: partner config parser.

---

### `WebServer`

Current status: good base.

It owns configs, creates listeners, registers them in the event loop, then runs the loop.

Current temporary weakness:

```txt
One ServerConfig -> one Listener
```

This is fine for now, but final code should group configs by unique `host:port`, because multiple server blocks can listen on the same interface/port.

Final target:

```txt
configs:
  server A listen 127.0.0.1:8080
  server B listen 127.0.0.1:8080
  server C listen 127.0.0.1:8081

listeners:
  Listener 127.0.0.1:8080 -> candidates [A, B]
  Listener 127.0.0.1:8081 -> candidates [C]
```

Dependency: final `ServerConfig` structure and getters.

---

### `Listener`

Current status: good base.

It creates the socket, sets `SO_REUSEADDR`, sets non-blocking mode, binds, listens, and accepts clients.

Keep:

- `socket()`
- `setsockopt(SO_REUSEADDR)`
- `fcntl(fd, F_SETFL, O_NONBLOCK)`
- `bind()`
- `listen()`
- `acceptClient()`

Be careful with `fcntl` improvements. In normal Linux code, people often use `F_GETFL` then `F_SETFL`. But the Webserv subject has a strict MacOS note about allowed `fcntl` flags. To stay subject-safe, prefer keeping your simple `F_SETFL, O_NONBLOCK` approach unless your team confirms a different interpretation with peers/evaluators.

Later improvement:

- Store more listener context, not only fd/host/port.
- The accepted client should know which listener accepted it, because routing/config selection may depend on the listening port.

---

### `EventLoop`

Current status: good base and probably the strongest part of your current work.

It already:

- stores all poll fds in one vector,
- stores listeners and clients separately,
- accepts clients when listener fd is readable,
- reads client only on `POLLIN`,
- writes client only on `POLLOUT`,
- switches event interest based on client state,
- removes finished clients,
- removes timed-out clients.

This is exactly the shape your part needs.

Main future changes:

1. `addClient(int clientFd)` should probably receive listener/config context.
2. Event loop may need to track CGI pipe fds if CGI is implemented asynchronously.
3. Poll event update should support extra states like `CLIENT_CGI_RUNNING`, `CLIENT_READING_FROM_CGI`, or a separate CGI fd map.

---

### `ClientConnection`

Current status: good prototype, but it still contains temporary HTTP bridge logic.

It already correctly owns:

- fd,
- read buffer,
- write buffer,
- state,
- last activity timestamp.

It already handles:

- partial reads,
- partial writes,
- switching from read to write,
- timeout support,
- close after response.

Temporary parts to remove or move later:

- direct HTTP parsing inside `processReadBuffer()`,
- manual header/body max checks in `ClientConnection`,
- local `reasonPhrase()` helper,
- direct call to `RequestHandler::handleRawRequest(_readBuffer)`,
- duplicate parsing with `RequestHandler`,
- hardcoded max body size in network layer.

Final goal:

```txt
ClientConnection reads bytes.
HttpRequestParser parses bytes.
RequestHandler/Router handles parsed request.
HttpResponseBuilder serializes response.
ClientConnection writes serialized bytes.
```

---

## 4. RequestHandler vs HttpRequestHandler

Right now they overlap too much.

Current structure:

```txt
RequestHandler
  raw string -> parse -> HttpRequest -> HttpRequestHandler -> raw response

HttpRequestHandler
  HttpRequest -> GET/POST/DELETE behavior -> HttpResponse
```

Final structure should be closer to:

```txt
RequestHandler / RequestDispatcher
  HttpRequest + RequestContext + Config
  -> choose server config
  -> choose location config
  -> check method
  -> decide redirect/static/upload/delete/CGI/error
  -> return HttpResponse

HttpRequestHandler
  Either remove it,
  or rename it to something more specific.
```

Possible better names for `HttpRequestHandler` later:

- `StaticFileHandler`
- `RouteExecutor`
- `ResourceHandler`
- `MethodHandler`

The important point: `RequestHandler` should not parse raw strings in final code if `ClientConnection` already used `HttpRequestParser`.

---

## 5. Clean target architecture for your part

```txt
WebServer
  owns final configs
  creates listener groups
  gives EventLoop access to listener contexts

Listener
  owns listening socket
  accepts client fds

EventLoop
  owns poll vector
  dispatches listener/client/cgi fd events
  owns ClientConnection objects

ClientConnection
  owns one client fd
  owns read/write buffers
  owns parser instance or parser state
  calls parser after receiving bytes
  stores pending response output

HttpRequestParser
  parses raw request data incrementally
  produces HttpRequest or error status

RequestHandler / Router
  uses parsed request + config context
  returns HttpResponse

HttpResponseBuilder
  converts HttpResponse into raw HTTP bytes
```

---

## 6. Ordered TODO list to finish your network/core part

### TODO 1 — Freeze the interfaces with partners - FINISHED

Do this before rewriting too much.

Agree on these structs/classes:

```cpp
class HttpRequest;
class HttpResponse;
class ServerConfig;
class LocationConfig;
class Router;
class RequestHandler;
```

Minimum needed from parser partner:

```cpp
enum ParseStatus
{
    PARSE_INCOMPLETE,
    PARSE_COMPLETE,
    PARSE_ERROR
};

class HttpRequestParser
{
public:
    ParseStatus parse(const std::string& buffer);
    const HttpRequest& getRequest() const;
    int getErrorStatusCode() const;
    std::size_t getConsumedBytes() const; // useful later for keep-alive/pipelining
    void reset();
};
```

Minimum needed from response partner:

```cpp
class HttpResponseBuilder
{
public:
    static std::string build(const HttpResponse& response);
};
```

Minimum needed from config/router partner:

```cpp
class RequestContext
{
public:
    const std::vector<ServerConfig>* serverCandidates;
    std::string localHost;
    int localPort;
};
```

This can be simple at first. The goal is to prevent every teammate from inventing a different boundary.

---

### TODO 2 — Remove duplicate parsing - FINISHED

Current problem:

```txt
ClientConnection parses raw buffer once.
RequestHandler::handleRawRequest() parses same raw buffer again.
```

Change final flow to:

```txt
ClientConnection
  -> parser.parse(_readBuffer)
  -> if complete: RequestHandler::handle(parser.getRequest(), context)
  -> HttpResponseBuilder::build(response)
```

Suggested final direction:

```cpp
HttpRequestParser::ParseStatus status = _parser.parse(_readBuffer);

if (status == HttpRequestParser::PARSE_INCOMPLETE)
    return;

if (status == HttpRequestParser::PARSE_ERROR)
{
    HttpResponse error = ErrorResponseFactory::create(_parser.getErrorStatusCode(), _context);
    _writeBuffer = HttpResponseBuilder::build(error);
    _state = CLIENT_WRITING;
    return;
}

HttpResponse response = _requestHandler.handle(_parser.getRequest(), _context);
_writeBuffer = HttpResponseBuilder::build(response);
_state = CLIENT_WRITING;
```

Do not copy-paste this blindly. Adapt it to your real class names.

Dependency: parser + response interface.

---

### TODO 3 — Move HTTP limits out of `ClientConnection`

Current temporary constants:

```cpp
MAX_HEADER_SIZE = 8192;
MAX_BODY_SIZE = 1000000;
```

Final direction:

```txt
Header sanity limit:
  parser-level or shared HTTP constant

Body max size:
  config-level value, usually client_max_body_size
```

`ClientConnection` can still protect itself from unlimited buffer growth, but HTTP-specific decisions should come from parser/config.

Dependency: config parser must expose `client_max_body_size`.

---

### TODO 4 — Keep subject-safe socket I/O behavior

Your rule for socket I/O should be:

```txt
recv only after POLLIN
send only after POLLOUT
no errno branching after recv/send
bytes <= 0 means close or finish client safely
```

Checklist:

- `EventLoop::handleClientEvent()` calls `readFromSocket()` only when `revents & POLLIN`.
- `EventLoop::handleClientEvent()` calls `writeToSocket()` only when `revents & POLLOUT`.
- `ClientConnection::readFromSocket()` does not call `recv()` in a loop unless your design is carefully subject-safe.
- `ClientConnection::writeToSocket()` handles partial send.
- Do not check `errno` after `recv()` or `send()`.
- Do not use blocking calls on sockets/pipes.

Current code is mostly okay here.

---

### TODO 5 — Add listener/context connection - FINISHED

Currently:

```cpp
void EventLoop::addClient(int clientFd);
```

Later:

```cpp
void EventLoop::addClient(int clientFd, Listener* listener);
```

or better:

```cpp
void EventLoop::addClient(int clientFd, const ListenerContext& context);
```

Why this matters:

```txt
Request comes from fd 7.
You need to know which listening port accepted fd 7.
That port maps to one or more ServerConfig objects.
Then RequestHandler/Router can choose correct config/location.
```

Dependency: final config/listener grouping.

---

### TODO 6 — Group listeners by unique host:port

Current:

```txt
for each ServerConfig -> create Listener
```

Final:

```txt
for each unique host:port -> create one Listener
attach all matching ServerConfig objects as candidates
```

This avoids bind errors if multiple server blocks use the same listen address.

Even though full virtual hosts are out of scope, multiple server blocks / multiple ports are part of the config requirement. A clean grouping also makes evaluation easier to explain.

Dependency: final `ServerConfig` data model.

---

### TODO 7 — Replace hardcoded config and root - FINISHED for temporary config stage

Temporary code to remove later:

```cpp
configs.push_back(ServerConfig("127.0.0.1", 8000));
configs.push_back(ServerConfig("127.0.0.1", 8001));
```

Temporary code to remove later:

```cpp
HttpRequestHandler handler("./www");
```

Final:

```txt
ConfigParser reads config file.
Router selects matching location.
Location provides root/upload path/index/autoindex/methods/cgi/redirect.
Handler uses selected location config.
```

Dependency: config parser and router.

---

### TODO 8 — Prepare RequestHandler as the integration point - FINISHED

Final `RequestHandler` should be the place where partner code connects.

Target responsibility:

```txt
RequestHandler::handle(request, context)
  -> select server config
  -> select best location
  -> validate method
  -> handle redirect if configured
  -> route to static/upload/delete/cgi
  -> return HttpResponse
```

It should not:

- read from sockets,
- write to sockets,
- parse raw bytes again,
- own client fd,
- know about poll directly.

Dependency: parser, config, router, response, static/upload/delete/CGI handlers.

---

### TODO 9 — Decide CGI event-loop design early

CGI is the most dangerous integration point for your part.

The subject allows `fork()` only for CGI. But CGI also uses pipes, and pipes are I/O descriptors that can wait for data. That means they should be non-blocking and driven by the same poll/equivalent loop.

Ask your partner this specific question:

```txt
Will CGI be blocking, or will it expose pipe fds so EventLoop can poll them?
```

Recommended design:

```txt
Request routes to CGI
  -> CgiHandler starts process and creates non-blocking pipes
  -> EventLoop registers CGI stdout fd, maybe CGI stdin fd
  -> ClientConnection waits in CGI state
  -> EventLoop reads CGI output when pipe fd is ready
  -> when CGI completes, build HttpResponse
  -> switch client to WRITING
```

You may need new maps in `EventLoop`:

```cpp
std::map<int, CgiProcess*> _cgiByFd;
std::map<int, int> _clientByCgiFd;
```

or a cleaner abstraction.

Dependency: CGI partner. This is one of the biggest “do not leave until the end” points.

---

### TODO 10 — Add final cleanup paths

Every client must be removed safely on:

- normal response completed,
- client disconnect,
- timeout,
- malformed request,
- body too large,
- internal handler failure,
- CGI failure,
- poll error/hangup.

Current `removeClient()` is good. Later make sure CGI resources are also cleaned if the client disappears while CGI is running.

Dependency: CGI process representation.

---

### TODO 11 — Keep simple close-after-response first

You do not need keep-alive immediately.

Current behavior:

```txt
send response
write buffer empty
CLIENT_DONE
close client
```

This is okay for a first stable mandatory version if your responses include:

```txt
Connection: close
```

Keep-alive can be added later only if everything else is stable.

If keep-alive is added later, you need:

- parser reset,
- consumed-byte tracking,
- leftover-buffer handling,
- request pipelining decisions,
- timeout reset,
- state reset to reading.

Do not start here.

---

### TODO 12 — Test your part without waiting for full project

Network/core tests you can already run:

```bash
curl -i --http1.1 http://127.0.0.1:8000/
curl -i --http1.1 http://127.0.0.1:8001/
printf 'GET / HTTP/1.1\r\nHost: localhost\r\n\r\n' | nc 127.0.0.1 8000
```

Slow client test:

```bash
nc 127.0.0.1 8000
```

Then type slowly:

```txt
GET / HTTP/1.1
Host: localhost

```

Expected behavior:

- server does not hang forever,
- timeout eventually closes stale clients,
- other clients can still connect while one client is slow.

Stress direction:

```txt
many clients connect
some send partial headers
some disconnect early
some request large body
some request invalid method/version
server stays alive
```

---

## 7. Dependency checklist: what you need from partners

### From HTTP parser / response partner

Need:

- stable `HttpRequest` fields:
  - method,
  - target/path,
  - query string,
  - version,
  - headers,
  - body,
  - maybe raw target.
- stable parser API:
  - incomplete / complete / error,
  - error status code,
  - consumed bytes if keep-alive later,
  - reset.
- Content-Length validation.
- chunked body handling or at least clear plan for CGI.
- stable `HttpResponse` fields:
  - status code,
  - reason phrase or status helper,
  - headers,
  - body or file target.
- final `HttpResponseBuilder::build(response)`.
- default error response builder/factory.

### From config/router partner

Need:

- `ConfigParser::parse()` returning validated configs.
- `ServerConfig` exposing:
  - host/interface,
  - port,
  - default root/index/error pages/body size,
  - locations.
- `LocationConfig` exposing:
  - path prefix,
  - allowed methods,
  - root,
  - index,
  - autoindex,
  - redirect,
  - upload allowed/path,
  - CGI extension/interpreter.
- route matching function:
  - best prefix match,
  - no regex needed.
- method validation result.

### From CGI partner

Need:

- Does CGI block or integrate into poll?
- How are CGI stdin/stdout pipe fds exposed?
- Who owns/kills/waits for CGI process?
- How does CGI return output to response builder?
- How are CGI timeouts handled?
- How are client disconnects handled while CGI is running?

For your network/core part, CGI integration is the highest-risk dependency.

---

## 8. What to delete or replace later

Delete/replace when integration starts:

- hardcoded `ServerConfig` entries in `main.cpp`,
- hardcoded `"./www"` in `RequestHandler`,
- `RequestHandler::handleRawRequest()` parsing raw request again,
- local `reasonPhrase()` in `ClientConnection`,
- generic `HttpRequestHandler` name if it becomes only static/resource handling,
- temporary `POST received: ...`,
- temporary `DELETE not implemented yet`,
- body/header limit constants in `ClientConnection`,
- unused `CLIENT_ERROR` if you never use it,
- unused `CLIENT_READING_BODY` if parser owns that state completely,
- unused `#include "routing/Router.hpp"` until real router integration.

---

## 9. Definition of done for your part

Your network/core part can be considered mostly done when:

- server starts from parsed config or clean config stub,
- one poll loop monitors all listener and client sockets,
- listener sockets are non-blocking,
- client sockets are non-blocking,
- no socket `recv/send` happens without poll readiness,
- no `errno` behavior branching after `recv/send`,
- multiple clients can connect at the same time,
- slow clients do not block other clients,
- disconnected clients are cleaned safely,
- timed-out clients are cleaned safely,
- partial writes work,
- response writing switches poll interest to `POLLOUT`,
- listener context can be passed to request handling,
- duplicate parsing is removed,
- request handling is behind a clean interface,
- CGI integration plan is agreed and event loop can support CGI fds if needed,
- you have tests for normal request, invalid request, disconnect, timeout, and many clients.

---

## 10. Recommended order from here

1. Agree final `HttpRequest`, `HttpResponse`, `ServerConfig`, `LocationConfig` interfaces with partners.
2. Remove duplicate parsing: parser result should be passed forward, not parsed again in `RequestHandler`.
3. Make `RequestHandler` accept `HttpRequest + RequestContext` instead of raw request string.
4. Move HTTP body/header limit logic out of `ClientConnection` or make it parser/config-driven.
5. Add listener context to clients.
6. Group listeners by unique host/port.
7. Replace hardcoded configs with `ConfigParser` when ready.
8. Replace hardcoded root/static handling with router/location result.
9. Decide and implement CGI poll integration plan.
10. Add cleanup for CGI + client disconnect cases.
11. Add stress/slow-client tests.
12. Only then consider keep-alive.

---

## 11. Simple explanation for evaluation

Use this explanation if someone asks why parser is connected to `ClientConnection`:

> `ClientConnection` does not parse HTTP by itself. It only owns the socket, buffers, and connection state. Because HTTP requests can arrive in multiple pieces, the connection stores or uses a parser instance and feeds it the bytes received from the socket. The parser decides whether the request is incomplete, complete, or invalid. Once it is complete, the parsed `HttpRequest` is passed to the request handling layer, which uses config and routing to create an `HttpResponse`. Then the response builder serializes it and the connection sends it incrementally when poll says the socket is writable.

That is the clean boundary.
