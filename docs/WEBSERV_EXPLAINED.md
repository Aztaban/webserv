# Webserv: how a request flows through it, and the technology behind it

Written from everything in `docs/` (READ_THIS, Architecture, Request_lifecycle, Project_development, the team split files, the testing checklist, the CGI design doc, Diana's and Martin's notes, the lifecycle diagram), checked against the real code on `main` (`af2f012`, re-checked 2026-09-28). The docs are mostly plans written before the code existed; the class and function names below are the ones that actually exist in the code.

## 1. What the project is

A small version of nginx in **C++98**. It's a program that listens on TCP ports, reads HTTP requests from browsers or curl, and answers them. The answer can be a file, a generated directory listing, an upload, a delete, a redirect, or the output of a script (CGI). The behaviour comes from a config file written in nginx style.

The hard rule that shapes the whole design: **the server must never block.** One `poll()` call watches every socket and pipe. The server only reads or writes a descriptor after `poll()` says it's ready, so one slow client or slow script can't freeze the others.

## 2. The big picture

Think of the server as a chain of separate machines, each doing one job:

```
 config file ──► ConfigParser ──► vector<ServerConfig>          (startup, once)
                                        │
                                        ▼
 WebServer ──► Listener (one per host:port) ──► EventLoop::run()  ◄── the one poll()
                                                   │
      ┌────────────────────────────────────────────┤
      ▼                                            ▼
 ClientConnection  (bytes in/out, state)       CgiJob  (child process + 2 pipes)
      │  raw bytes                                 ▲
      ▼                                            │ CgiRequest
 HttpRequestParser ──► HttpRequest                 │
      │                                            │
      ▼                                            │
 RequestHandler ──► Router::matchLocation ──► RequestResult
      │                                   ├─ RESPONSE_READY → HttpResponse
      │                                   └─ CGI_REQUIRED   → CgiRequest ──┘
      ▼
 HttpResponseBuilder::build() ──► raw bytes ──► ClientConnection write buffer
```

The docs sum it up in one line: **Config = rules, Request = what the client wants, Router = applies the rules to the request.**

Ownership:
- **Martin:** network (`WebServer`, `Listener`, `EventLoop`, `ClientConnection`)
- **Diana:** HTTP (the parser, `HttpRequestHandler` for GET/POST/static files/autoindex, and the response builder)
- **Lada:** config, routing, redirects, DELETE and CGI

## 3. One request, step by step

### Startup (`src/main.cpp`)
1. `main` ignores `SIGPIPE`. Without this, writing to a socket the client already closed would kill the whole process.
2. `ConfigParser` runs `loadSource()`, then `tokenize()`, then `parse()`, and hands back a `std::vector<ServerConfig>`. Each server config holds its `LocationConfig`s.
3. `WebServer::setup()` creates one `Listener` per unique `host:port` using `socket()`, `bind()` and `listen()`, and makes each one non-blocking. If two server blocks share a port, they become one listener with several "server candidates".
4. `EventLoop::run()` (`src/network/EventLoop.cpp`) loops forever. Each pass:
   - checks CGI child processes (`_checkCgiProcesses`),
   - calls `poll()` (timeout 1 s, or 100 ms while CGI jobs run),
   - removes timed-out CGI jobs (504) and idle clients,
   - sends each ready fd to the right handler: listener, client, or CGI pipe.

### Accept
5. When a listener is readable, `_handleListenerEvent` calls `accept()` until nothing is left. For each new client it creates a `ClientConnection` holding a `RequestContext` (which port it came in on and which configs apply), then registers the client fd for `POLLIN`.

### Read and parse (Martin + Diana)
6. When the client is readable, `readFromSocket()` does one `recv()` and appends the bytes to `_readBuffer`. The code never assumes the whole request arrived at once.
7. `_processReadBuffer()` (`src/network/ClientConnection.cpp`) runs the parser over the buffer. The result is `PARSE_INCOMPLETE` (wait for more bytes), `PARSE_ERROR` (build a 400/501/505), or `PARSE_COMPLETE`. Once the headers are complete, it checks `Content-Length` against the selected server's `client_max_body_size` and answers 413 if it's too big.

### Route and decide (Lada)
8. `RequestHandler::handle()` (`src/core/RequestHandler.cpp`):
   - **Picks the server block:** `Router::matchServer` compares the `Host` header against each block's `server_name`, falling back to the first block.
   - **Picks the location:** `Router::matchLocation` does a longest-prefix match, so `/images/private/a.png` matches `/images/private` before it matches `/images`. No match gives 404.
   - **Decides what to do, in this order:**
     - The location has `return 301 /x;`: build a redirect with a `Location` header.
     - The method isn't in the location's `methods`: 405 with an `Allow` header.
     - The method is `DELETE`: `stat()` the file under the location's `root` and `remove()` it. The answer is 204, 404, or 403 (for directories or paths containing `..`).
     - The extension is listed in `cgi_extension`: return `RequestResult::fromCgiRequest(...)`.
     - `POST` to a location without `upload on`: 403.
     - Anything else goes to Diana's `HttpRequestHandler` with the location's `root` (location prefix stripped), `index`, `autoindex` and `upload_path`: a static file, an index file, autoindex, or an upload.

### Respond
9. `prepareResponse()` swaps in the configured `error_page` for any 4xx/5xx, turns the `HttpResponse` into bytes with `HttpResponseBuilder::build()` and switches the client to `CLIENT_WRITING`. The event loop then watches that fd for `POLLOUT`.
10. `writeToSocket()` calls `send()` and erases whatever was actually sent. A partial write is fine: the rest goes out on the next `POLLOUT`. When the buffer is empty, the client moves to `CLIENT_DONE`, which means close. There's no keep-alive yet.
11. `_removeClient` removes the fd from the poll list, closes it and deletes the object. This same cleanup runs on errors, disconnects (`POLLHUP`/`POLLERR`) and timeouts, so every path ends in a clean state.

## 4. The CGI branch

CGI means the server runs an external program and sends that program's output back as the response. The key design choice is that **CGI is a job driven by the event loop, not a function call.** A function call would block the whole server while the script runs.

```
RequestHandler → CGI_REQUIRED + CgiRequest (script, interpreter, env, body)
ClientConnection → CLIENT_WAITING_CGI      (socket stays open, nothing is read or written on it)
EventLoop::_startCgiForClient → CgiJob::start():
    realpath(script) → pipe() ×2 (FD_CLOEXEC, parent ends O_NONBLOCK) → fork()
        child:  dup2 pipes onto stdin/stdout, chdir(script dir), execve(python3 script)
        parent: close the child's ends
    stdout pipe → poll(POLLIN), stdin pipe → poll(POLLOUT) only if there's a body
EventLoop::_handleJobEvent:
    POLLOUT → writeToCgi() (a piece of the body)    POLLIN → readFromCgi() (append output)
    closed pipes are removed from poll right away
EventLoop::_checkCgiProcesses (every loop pass):
    waitpid(WNOHANG) → child exited and stdout at EOF → CgiOutputParser turns
    "Status:/Content-Type: + blank line + body" into an HttpResponse (500 if the script failed)
    → client->prepareResponse() → job released
EventLoop::_removeTimedOutCgi: job older than CGI_TIMEOUT_SECONDS → kill, 504 to the client
```

Three maps connect pipes to clients:
- `_cgiByFd`: pipe → job
- `_cgiFdToClientFd`: pipe → client
- `_cgiByClientFd`: client → job

If the client disconnects mid-job, `_removeClient` → `_releaseCgi` kills the job, so there's no dangling pointer. A killed child that isn't reapable yet is parked in `_retiredCgiJobs` and reaped on a later pass, so no zombie process is left behind.

**Why you can't write the whole body first and then read the output:** a pipe's buffer only holds about 64 KB. If the script's output fills the stdout pipe while the server is still blocked writing to stdin, each side waits on the other forever. That's a deadlock. Polling both pipes independently avoids it.

## 5. The technology topics, briefly

| Topic | What to know |
|---|---|
| **TCP sockets** | `socket` → `bind` → `listen` → `accept`. Each `accept` returns a new fd for one client. |
| **Non-blocking I/O + `poll()`** | `O_NONBLOCK` makes a call return immediately instead of waiting. `poll()` reports which fds are ready. The subject forbids looking at `errno` after `read`/`write`, which is why the code treats `bytes <= 0` as "done or disconnected". |
| **State machine** | Each connection is in exactly one state (`READING_HEADERS`, `READING_BODY`, `PROCESSING`, `WAITING_CGI`, `WRITING`, `DONE`). The state decides which poll events the fd asks for. |
| **HTTP/1.1 format** | A request line, then headers, then an empty line (`\r\n\r\n`), then an optional body. The body length comes from `Content-Length` or from `Transfer-Encoding: chunked` (hex size, data, repeat, `0` ends it). A response is a status line, headers and a body. |
| **Status codes** | 200/201/204 success · 301/302 redirect · 400 bad syntax · 403 forbidden · 404 missing · 405 method not allowed · 413 body too large · 500 server error · 502 bad CGI output · 504 CGI timeout |
| **Config parsing** | Two stages. The tokenizer (`TokenStream`) splits the text into words, `{`, `}` and `;`. A recursive-descent parser (`parseServer`, `parseLocation`, `parseDirective`) turns those tokens into objects and validates them. |
| **Virtual hosts** | Several server blocks can share one port. The `Host:` header picks between them by `server_name`, and the first block is the default. |
| **Location matching and root** | Longest prefix wins, then the URL is mapped to a path on disk using that location's `root`. Watch out for `..` path traversal. |
| **CGI** | `fork`/`execve`/`pipe`/`dup2`. Request data goes to the script as environment variables (`REQUEST_METHOD`, `QUERY_STRING`, `CONTENT_LENGTH`…) and the body goes to its stdin. The script prints headers without a status line, and the server adds one. |
| **Processes** | You need `waitpid` or you get zombies, and `kill` on timeout. `fork` is only allowed for CGI. |
| **MIME types** | File extension → `Content-Type`, so the browser knows how to render the response. |
| **Keep-alive** | HTTP/1.1 reuses connections by default. This project closes after every response, which is allowed and simpler. |

## 6. Where the docs and the code differ (as of 2026-09-28, `main`)

- **Class names:** the docs mention `CgiHandler`, `ResourceResolver`, `MimeTypes` and `PathUtils`. The code actually uses `CgiJob`, `CgiOutputParser`, `HttpRequestHandler`, `ErrorResponseFactory` and `Router`. The config parser is split into `ConfigParser`, `ConfigParserBlocks` and `ConfigParserUtils`. `EffectiveRouteConfig` exists in `include/core/` but isn't used yet.
- **Gaps from the old `lformank` branch, now fixed on `main`:**
  - server selection by `Host`
  - location `methods` → 405
  - the location's `root`, `index`, `autoindex` and `upload_path`, and `error_page`, are used
  - the CGI timeout answers 504
  - the `errno` checks after pipe I/O were removed
  - DELETE returns a proper 204
- **Still open:** see section 8.9.

## 7. Suggested reading order

1. `docs/READ_THIS.md`: the concepts
2. `docs/Request_lifecycle.md`
3. `docs/lada/CGI_event_loop_design.md`
4. `docs/martin:P/Martin.md`: lists what the network layer expects from the config/CGI side

Then read the code in the order of section 3, starting at `src/main.cpp`.

---

## 8. Evaluation sheet: questions and answers

These are the checks from the official webserv evaluation scale, each followed by the answer for **`main` @ `af2f012`**, re-checked on 2026-09-28.

How it was checked: after a clean `make re`, the server was run and tested with curl against `conf/default.conf`, plus temporary configs (virtual hosts, a custom `error_page`, a body limit of 10, and CGI scripts that crash, omit headers or loop forever), a 3,000-request parallel curl burst, and a valgrind run. `siege` isn't installed on this machine, so that item still needs a real siege run.

Evidence tags:
- **Verified live:** tested against the running server.
- **Code-read:** checked in the source, not run.

Status tags:
- **Ready:** works as the code stands.
- **Risk:** works partly, or an evaluator could reasonably fail it.
- **Missing:** not implemented.

Ownership tags (from the team split in `docs/READ_THIS.md` and `docs/Final_team_split.md`):
- **[Lada]:** config parser, virtual server selection, location matching, redirects, DELETE, CGI.
- **[Martin]:** network core (`WebServer`, `Listener`, `EventLoop`, `ClientConnection`), poll loop, sockets, timeouts, stress.
- **[Diana]:** HTTP parser, response builder, static files, MIME types, uploads (POST), autoindex, error pages.
- **[Team]:** everyone must be able to answer it (the sheet flags "can't explain code" per student).
- Shared tags like **[Lada + Diana]** have a note saying who owns which piece.

Questions per person:
- **Lada:** 8.1 errno and return values (CGI pipes) · 8.2 status codes, ports, hostnames, error page, body limit, routes, default file, methods · 8.3 GET/POST/DELETE (DELETE), status codes · 8.4 all of CGI · 8.5 directory listing (config flag), redirect · 8.6 ports, same port twice · 8.7 hanging connections (CGI) · 8.8 more than one CGI.
- **Martin:** 8.1 multiplexing, how poll works, one poll, read/write at the same time, one read/write per client, removing clients on error, return values and errno (sockets), no fd outside poll · 8.2 ports, body limit · 8.3 unknown requests (no crash) · 8.6 ports, same port twice, common ports across servers · 8.7 all of siege & stress.
- **Diana:** 8.2 status codes, error page, routes, default file · 8.3 GET/POST/DELETE (GET, POST), unknown requests (parser), status codes, upload and get back · 8.5 headers, static site, wrong URL, directory listing.
- **Team:** 8.1 HTTP basics, compilation/relink · 8.7 memory leaks · 8.8 cookies (unassigned bonus).

### 8.1 Check the code and ask questions

**Q: Explain the basics of an HTTP server.** **[Team]**
A client opens a TCP connection and sends a text request (a request line, headers, a blank line and an optional body). The server parses it, decides what to do based on its config (serve a file, run CGI, upload, delete, redirect, error), and sends back a response (a status line, headers, a blank line and a body). Our server does this for many clients at once, non-blocking, with one `poll()` loop. Sections 2 and 3 above walk through the full path. **Ready**

**Q: Which function did you use for I/O multiplexing?** **[Martin]**
`poll()`, in `EventLoop::run()` (`src/network/EventLoop.cpp`). **Ready** (code-read)

**Q: How does `select()` (or the equivalent) work?** **[Martin]**
We hand `poll()` an array of `struct pollfd`, one per fd, each with the events we care about (`POLLIN` for "ready to read", `POLLOUT` for "ready to write"). The kernel puts the process to sleep until at least one fd is ready or the timeout runs out. The timeout is 1000 ms, or 100 ms while CGI jobs are running, so finished child processes get noticed quickly. `poll()` then fills in `revents` for every fd that's ready, and we loop over those and dispatch each one. **Ready** (code-read)

**Q: Do you use only one `poll()`, and how does the server accept clients and read/write them?** **[Martin]**
Yes, one `poll()` in the main loop. Every fd is in the same `_pollFds` vector: listener sockets, client sockets and CGI pipes. Dispatch goes by fd type:
- **Listener with `POLLIN`:** `accept()` → a new `ClientConnection`, registered for `POLLIN`.
- **Client:** `readFromSocket()` on `POLLIN`, `writeToSocket()` on `POLLOUT`.
- **CGI pipe:** `writeToCgi()` / `readFromCgi()` in `_handleJobEvent`.

Each iteration also runs `_checkCgiProcesses()` (a non-blocking `waitpid(WNOHANG)`, no I/O) and the timeout sweeps. `_updateClientEvents()` changes which events a client asks for based on its state. **Ready** (code-read)

**Q: Does `poll()` check read and write AT THE SAME TIME?** (Otherwise the grade is 0.) **[Martin]**
Yes. One `poll()` call watches every fd. Each fd asks for `POLLIN` or `POLLOUT` depending on its state (reading clients and CGI stdout pipes ask for `POLLIN`; writing clients and CGI stdin pipes ask for `POLLOUT`), so reads and writes are multiplexed in the same call. **Ready** (code-read)

**Q: Is there only one read or one write per client per `poll()`? Show the code from `poll()` to the client read/write.** **[Martin]**
Walk through `EventLoop::run()` → `_handleClientEvent()` → `ClientConnection::readFromSocket()` (one `recv()`) or `writeToSocket()` (one `send()`), both in `src/network/ClientConnection.cpp`. A client only ever asks for `POLLIN` *or* `POLLOUT`, never both, so at most one of the two runs per iteration. **Ready** (code-read)
- **Risk:** `_handleListenerEvent` still calls `accept()` in a loop until it returns -1. The later `accept()` calls happen without a fresh readiness report. Some evaluators flag this; accepting a single client per `POLLIN` is the strictly safe version.

**Q: If read/recv/write/send on a socket returns an error, is the client removed?** **[Martin]**
Yes. `recv()`/`send()` returning `<= 0` sets the state to `CLIENT_DONE`, and `_handleClientEvent` then calls `_removeClient()`. That removes it from poll, closes the fd, releases any CGI job and deletes the object. `POLLERR`/`POLLHUP`/`POLLNVAL` also remove the client. **Ready** (code-read)

**Q: Is the return value checked properly (both -1 and 0, not just one)?** **[Lada + Martin]** (Lada: the CGI pipe reads/writes in `CgiJob`; Martin: client sockets)
- **Client sockets:** `bytes <= 0` covers both.
- **`CgiJob::writeToCgi()`:** `> 0` advances; anything else (0 or -1) fails the job.
- **`CgiJob::readFromCgi()`:** `> 0` appends, `0` is EOF (close stdout), `-1` fails the job.

**Ready** (code-read)

**Q: Is `errno` checked after read/recv/write/send?** (If yes, the grade is 0.) **[Lada + Martin]** (Lada: `CgiJob`; Martin: client sockets)
**No. Fixed on `main`:** the old `errno == EAGAIN` checks in `CgiJob` are gone. `errno` now appears in only two places, neither after I/O. Be ready to explain both:
- `CgiJob::updateProcessState()` checks `errno` after `waitpid()` (there's a comment saying so).
- `ConfigParser` checks `errno == ERANGE` after `strtol()`.

**Ready** (code-read with `grep -rn errno src`)

**Q: Is ANY fd read or written without going through `poll()`?** **[Martin + Lada + Diana]** (Martin: sockets; Lada: CGI pipes; Diana: direct disk file reads/writes)
Sockets and CGI pipes always go through `poll()`. Regular disk files are read and written directly: `FileUtils::read` and `ifstream` for pages, `ofstream` for uploads, `opendir`/`readdir` for autoindex. The subject explicitly says regular disk files don't need `poll()`, so have that sentence ready. **Ready** (defend it)

**Q: Does it compile without relinking?** **[Team]**
Verified live: a second `make` prints "Nothing to be done for 'all'". Flags are `-Wall -Wextra -Werror -std=c++98`.
- **Risk:** the `Makefile` defines `DEPS` but never `-include`s it, so editing only a header doesn't rebuild the objects that use it. After pulling `main`, the stale objects made **`./webserv` segfault at startup** until a clean `make re`. An evaluator starting from a fresh `git clone` won't hit this, but fix it (`-include $(DEPS)`) and always run `make re` after a pull.

### 8.2 Configuration

**Q: Are the HTTP status codes correct?** **[Lada + Diana]** (Lada: routing, DELETE and CGI codes; Diana: `ErrorResponseFactory` and the response builder)
Verified live:

| Situation | Code |
|---|---|
| GET a file | 200 |
| Upload | 201 |
| DELETE | 204 No Content (no body, no `Content-Length`) |
| Redirect | 301 |
| Missing file | 404 |
| Method not allowed | 405 with `Allow: GET` |
| Body too large | 413 |
| Unknown method | 501 |
| HTTP/1.0 request | 505 |
| CGI crash | 500 |
| CGI infinite loop | 504 |

**Risk:**
- A CGI script that doesn't exist gives **500** (should be 404; `_isCgiRequest` never checks that the file exists).
- CGI output without `Content-Type` gives 500 (502 fits better).
- A directory with no index and `autoindex off` gives 404 (nginx uses 403).
- `HTTP/1.0` requests get **505**, but nginx accepts them. An evaluator testing with `telnet` and `HTTP/1.0` will see an error.

**Q: Multiple servers with different ports.** **[Lada + Martin]** (Lada: parsing `listen`; Martin: creating the listeners)
Supported: `conf/multi_port.conf` has 8000 → `www/site1` and 8001 → `www/site2`. `WebServer::_createListeners` creates one listener per `host:port`. **Ready** (verified live with two ports)

**Q: Multiple servers with different hostnames** (`curl --resolve example.com:80:127.0.0.1 http://example.com/`). **[Lada]** (virtual server selection)
**Fixed on `main`.** `Router::matchServer` compares the `Host` header (with `:port` stripped, case-insensitive) against every block's `server_name`, and falls back to the first block. The selected server is then used for routing, the body limit and error pages. Verified live: two blocks on port 8100, `alpha.test` → site1 and `beta.test` → site2. **Ready**

**Q: Default error page (try changing the 404).** **[Lada + Diana]** (Lada: parsing `error_page`; Diana: serving error pages)
**Fixed on `main`.** `ErrorResponseFactory::applyCustomPage` loads the file from `error_page <code> <path>`, and it runs for every 4xx/5xx (static, parser and CGI errors) in `ClientConnection::prepareResponse`. The status code stays the same. If there's no `error_page`, it falls back to `<root>/errors/<code>.html`, then to a generated page. Verified live: pointing `error_page 404` at a different file served that file with status 404. **Ready**

**Q: Limit the client body** (`curl -X POST -H "Content-Type: plain/text" --data "..."`). **[Lada + Martin]** (Lada: parsing `client_max_body_size`; Martin: the 413 check in `ClientConnection`)
The limit is checked against `Content-Length` once the headers are parsed, and against the bytes received so far. It uses the body limit of the server block selected by `Host`. Verified live: with a limit of 10, the sheet's exact curl command gives 413; a 2 MB upload against the 1 MB default gives 413. **Ready**

**Q: Routes in a server pointing to different directories.** **[Lada + Diana]** (Lada: location matching and passing the location's `root` on; Diana: resolving file paths in `HttpRequestHandler`)
**Fixed on `main`.** `RequestHandler` uses the matched location's `root` and strips the location prefix (like nginx `alias`), so `location /uploads { root ./www/site1/uploads; }` maps `/uploads/f.txt` → `./www/site1/uploads/f.txt`. Verified live. **Ready** (be ready to explain that the prefix is stripped, which differs from nginx `root`).

**Q: A default file to serve when a directory is requested.** **[Lada + Diana]** (Lada: the `index` directive; Diana: directory handling in `_handleGET`)
**Fixed on `main`.** The location's `index` is used, falling back to the server's `index`. Verified live: `GET /listing/` serves `listing/index.html`. **Ready**

**Q: A list of accepted methods for a route** (e.g. delete something with and without permission). **[Lada]** (routing and DELETE)
**Fixed on `main`.** `RequestHandler::handle` checks `location->methods` and returns 405 with an `Allow` header. Verified live:
- `POST /` and `DELETE /index.html` (location allows only `GET`) → **405**, `Allow: GET`.
- `DELETE /uploads/f.txt` (location allows `GET DELETE`) → **204**.

POST also needs `upload on` in the location, otherwise it gets 403. **Ready**

### 8.3 Basic checks

**Q: GET, POST and DELETE requests work.** **[Lada + Diana]** (Lada: DELETE; Diana: GET and POST)
Verified live:
- **GET:** static files, index files, autoindex. **Ready**
- **POST:** `POST /upload/f.txt` → 201, saved into the location's `upload_path`. **Ready**
  - The file name is the last URL segment. `POST /upload` with no name saves a file literally called `upload`.
- **DELETE:** existing file → 204; again → 404; directory or `..` → 403; not allowed → 405. **Ready**

**Q: UNKNOWN requests do not crash the server.** **[Diana + Martin]** (Diana: parser validation; Martin: `SIGPIPE` and connection handling)
Verified live: `FOO /` gives 501 Not Implemented and the server keeps running. Other cases:
- Malformed request lines/headers or a bad `Host` → 400.
- Wrong HTTP version → 505.
- Oversized headers → 431.
- `SIGPIPE` is ignored, so a client disconnecting mid-write can't kill the process.

**Ready**

**Q: Every test returns the appropriate status code.** **[Lada + Diana]** (Lada: routing, DELETE, CGI; Diana: parser and error responses)
See the table in 8.2. All the main paths are correct. The remaining risks are a missing CGI script (500 instead of 404) and `HTTP/1.0` getting 505. **Risk (small)**

**Q: Upload a file to the server and get it back.** **[Diana]**
**Fixed on `main`.** Verified live with `conf/default.conf`: `POST /upload/f.txt` (201) → `GET /uploads/f.txt` returns the content → `DELETE /uploads/f.txt` (204) → `GET` gives 404. **Ready**

### 8.4 Check CGI

**Q: The server works fine with CGI.** **[Lada]**
Verified live: `GET /cgi-bin/hello.py?name=Lada` → 200 with the HTML showing `Method: GET` and `Name: Lada`; `POST /cgi-bin/echo.py` → 200. `hello.py` was broken on the old branch and is fixed on `main`. Scripts run as non-blocking `CgiJob`s whose pipes are polled by the same event loop (see section 4). **Ready**

**Q: CGI runs in the correct directory for relative file access.** **[Lada]**
Yes. The script path is resolved with `realpath()` before `fork()`, and the child calls `chdir()` into the script's directory before `execve()`; if `chdir` fails, the child exits and the client gets 500. **Ready** (code-read)

**Q: Test CGI with GET and POST.** **[Lada]**
Verified live:
- **GET:** `REQUEST_METHOD` and `QUERY_STRING` reach the script.
- **POST:** the body is written to the script's stdin, in pieces, whenever the pipe is writable, and `CONTENT_LENGTH`/`CONTENT_TYPE` are set.

Demo:
```
curl "http://127.0.0.1:8001/cgi-bin/hello.py?name=x"
curl -X POST --data "a=1" http://127.0.0.1:8001/cgi-bin/echo.py
```
**Ready**

- **Risk (code-read):** `_buildCgiRequest` reuses a `stringstream` after `ss.clear()`, which only clears error flags and doesn't empty the stream. As a result:
  - `SERVER_NAME` comes out as `127.0.0.1/<content-length><port>` (e.g. `127.0.0.1/08001`).
  - There's no `SERVER_PORT`.
  - `CONTENT_TYPE` is only found if the header is spelled exactly `Content-Type`.

  The demo scripts don't use these variables, but an evaluator printing the environment would see them.

**Q: Scripts with errors or infinite loops are handled; the server never crashes and an error is visible.** **[Lada]**
Verified live:
- **Script exits with an error (`exit 3`):** 500 "CGI failed". **Ready**
- **Output without `Content-Type`:** 500. **Ready**
- **Infinite loop (`while True: pass`):** **504 Gateway Timeout** after 30 s. The child process is killed and reaped (no zombie left). While it ran, other requests (static and CGI) were answered in 1–25 ms. **Ready**
  - The timeout is `CGI_TIMEOUT_SECONDS = 30`. That's a long wait during a defence; 5–10 s is friendlier.
- **Script that doesn't exist:** 500 instead of 404. **Risk**

### 8.5 Check with a browser

**Q: Look at the request and response headers in the browser's network tab.** **[Diana]**
Verified live:
- Every response has `Content-Length`, `Content-Type` and `Connection: close` (the server closes after every response, so the header is honest).
- 405 responses add `Allow`.
- 204 responses have no body and no `Content-Length`.
- Redirects carry `Location`.

**Ready**

**Q: It can serve a fully static website.** **[Diana]**
`www/site1` and `www/site2` are full demo sites with CSS and images. MIME types come from `FileUtils::mimeType`. **Ready** (open them in the browser before the defence)

**Q: Try a wrong URL.** **[Diana]**
Verified live: 404 with the custom styled 404 page from `error_page`. **Ready**

**Q: Try to list a directory.** **[Lada + Diana]** (Lada: passing the location's `autoindex` flag on; Diana: generating the listing)
**Fixed on `main`:** `autoindex` now comes from the location. Verified live:
- `GET /uploads/` (`autoindex on`) → 200 with a listing. Links are absolute and HTML-escaped.
- A directory without an index under `autoindex off` → 404.

**Ready**

**Q: Try a redirected URL.** **[Lada]**
Verified live: `GET /old` → `301 Moved Permanently` with `Location: /`. The config parser only accepts 301/302 for `return`. **Ready**

### 8.6 Port issues

**Q: Multiple ports with different websites; the browser shows the right site.** **[Lada + Martin]** (Lada: the config; Martin: the listeners)
Use `conf/multi_port.conf`: `127.0.0.1:8000` → site1, `127.0.0.1:8001` → site2. **Ready**

**Q: Set up the same port multiple times in the config. It should not work.** **[Lada + Martin]** (Lada: config validation; Martin: grouping blocks into listeners)
Verified live: two blocks with the same `host:port` **and the same `server_name`** are accepted, and the server starts; the second block can never be reached. **Risk**
- Different `server_name`s on one port are correct virtual hosts, so explain the grouping.
- The parser already rejects a duplicate `listen` inside one block, duplicate location paths, and duplicate `server_name` values within a block. Rejecting the same `host:port` + `server_name` across blocks would close this gap.

**Q: Run multiple servers at the same time with different configs but common ports.** **[Martin]**
Verified live: the second process logs `webserv: bind() failed` and exits with 1; the first server keeps working. If asked why: a port can only be bound by one listening socket at a time. **Ready**

### 8.7 Siege & stress test

**Q: Availability above 99.5% for `siege -b` on an empty page.** **[Martin]**
`siege` isn't installed here. Stand-in: 3,000 GETs with 50 in parallel via curl gave **3,000 × 200** (100%). **Install siege and run `siege -b` before the defence.**

**Q: No memory leaks (memory usage doesn't keep growing).** **[Team]** (Martin: client objects and fds; Lada: `CgiJob`s and CGI pipes)
Verified live:
- **Valgrind** over 15 mixed requests (GET, 404, CGI, upload, DELETE): **0 errors; definitely, indirectly and possibly lost all 0**.
- **After the 3,000-request burst:** the process had **4 fds open** (stdio + listener) and about 4 MB of resident memory.
- The "99 KB still reachable" appears because Ctrl+C kills the process with the default `SIGINT` action. There's no graceful shutdown yet (`include/utils/SignalState.hpp` exists but nothing uses it). This isn't a leak; say so if asked.

**Ready**

**Q: No hanging connections.** **[Lada + Martin]** (Lada: the CGI timeout; Martin: the client timeout)
Every connection closes after its response, and idle clients are removed after 30 s. Clients waiting on a CGI script are exempt from the idle timeout because the CGI timeout answers them with 504 (verified live). **Ready**

**Q: Siege can run indefinitely without restarting the server.** **[Martin]**
The fd count and memory stayed flat after 3,000 requests. **Ready** (confirm with a long `siege -b` run)

### 8.8 Bonus

**Q: Working cookies and session system.** **[Team]** (unassigned bonus; closest fit is Diana, since cookies are headers)
**Missing.** Nothing in `src/` or `include/` handles cookies or sessions.

**Q: More than one CGI system.** **[Lada]**
The parser accepts several extension/interpreter pairs in one directive (`cgi_extension .py /usr/bin/python3 .sh /bin/sh;`), and `LocationConfig::cgiExtensions` maps extension → interpreter. Only `.py` scripts exist and are configured, though. **Risk (not demoed)**

### 8.9 Before the defence: fixes, most important first

1. **[Team]** Add `-include $(DEPS)` to the `Makefile`, and always `make re` after pulling (stale objects crashed the server at startup).
2. **[Lada]** Missing CGI script → 404: check that the file exists before returning `CGI_REQUIRED`.
3. **[Lada]** Fix the CGI env: use `ss.str("")` between uses, set `SERVER_NAME=<host>`, add `SERVER_PORT`, and look up `Content-Type` case-insensitively.
4. **[Lada]** Lower `CGI_TIMEOUT_SECONDS` (e.g. 5–10 s) so the infinite-loop demo doesn't take 30 s.
5. **[Lada + Martin]** Reject two server blocks with the same `host:port` and the same `server_name`.
6. **[Diana]** Decide on `HTTP/1.0` → 505 (nginx accepts 1.0; telnet demos often use it).
7. **[Martin]** Optional: accept one client per `POLLIN`; handle `SIGINT` for a clean shutdown (clean valgrind output).
8. **[Lada]** Bonus: add a second CGI type (e.g. `.sh`) and a demo script.
9. **[Martin + Team]** Install siege and run `siege -b` on an empty page.
