# Webserv: how a request flows through it, and the technology behind it

Written from everything in `docs/` (READ_THIS, Architecture, Request_lifecycle, Project_development, the team split files, the testing checklist, the CGI design doc, Diana's and Martin's notes, the lifecycle diagram), checked against the real code on branch **`eval-fix` @ `31cfc37`** (re-checked 2026-10-05). The docs are mostly plans written before the code existed; the class and function names below are the ones that actually exist in the code.

Sections 1–7 explain how the server works. Section 8 is the evaluation sheet: every question has a **→ §** pointer back to the part of the explanation that answers it, so you can learn the mechanism once and reuse it for several questions.

## 1. What the project is

A small version of nginx in **C++98** (`-Wall -Wextra -Werror -std=c++98`). It's a program that listens on TCP ports, reads HTTP requests from browsers or curl, and answers them. The answer can be a file, a generated directory listing, an upload, a delete, a redirect, or the output of a script (CGI). The behaviour comes from a config file written in nginx style.

```
./webserv                    # uses conf/default.conf
./webserv conf/eval.conf     # config for the subject's tester (YoupiBanane + cgi_tester)
```

The hard rule that shapes the whole design: **the server must never block.** One `poll()` call watches every socket and pipe. The server only reads or writes a descriptor after `poll()` says it's ready, so one slow client or slow script can't freeze the others. Everything runs in one process and one thread; the only other processes are CGI children.

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
 ClientConnection  (bytes in/out, state)       CgiJob  (child process + pipes)
      │  raw bytes                                 ▲
      ▼                                            │ CgiRequest
 HttpRequestParser ──► HttpRequest                 │
      │                                            │
      ▼                                            │
 Router::matchServer ──► RequestHandler ──► Router::matchLocation ──► RequestResult
                                          ├─ RESPONSE_READY → HttpResponse
                                          └─ CGI_REQUIRED   → CgiRequest ──┘
      ▼
 HttpResponseBuilder ──► raw bytes ──► ClientConnection write buffer
```

The docs sum it up in one line: **Config = rules, Request = what the client wants, Router = applies the rules to the request.**

Where the code lives:

| Folder | What's in it | Owner |
|---|---|---|
| `src/config/` | `ConfigParser`, `TokenStream`, `ServerConfig`, `LocationConfig` | Lada |
| `src/routing/` | `Router`: server by `Host`, location by longest prefix | Lada |
| `src/core/` | `WebServer`, `RequestHandler` (what is this request?), `ErrorResponseFactory` | Martin / Lada / Diana |
| `src/cgi/` | `CgiJob` (fork/execve/pipes), `CgiOutputParser`, `CgiRequest` | Lada |
| `src/network/` | `Listener`, `EventLoop` (the `poll()` loop), `ClientConnection` (one client) | Martin |
| `src/http/` | `HttpRequestParser`, `HttpRequestHandler` (static GET/POST/autoindex), `HttpResponseBuilder` | Diana |
| `src/utils/` | `Logger`, `FileUtils` (disk read/write, MIME types) | shared |

## 3. One request, step by step

### 3.1 Startup (`src/main.cpp`, `src/core/WebServer.cpp`)
1. `main` ignores `SIGPIPE`. Without this, writing to a socket the client already closed would kill the whole process.
2. `ConfigParser` runs `loadSource()`, then `tokenize()`, then `parse()`, and hands back a `std::vector<ServerConfig>` (§3.2).
3. `WebServer::_createListeners()` groups the server blocks by `host:port` in a `std::map`. Each group becomes **one** `Listener` holding all its blocks as "server candidates". This is how virtual hosts work: several blocks share one socket and the `Host` header picks between them later (§3.6).
4. `Listener::setup()`: `socket()` → `setsockopt(SO_REUSEADDR)` → `fcntl(O_NONBLOCK)` → `getaddrinfo()` → `bind()` → `listen(SOMAXCONN)`.
5. `EventLoop::run()` loops forever (§3.3). Any exception during startup is printed as `webserv: <reason>` and `main` returns 1.

### 3.2 The config parser (Lada)

Two stages.

**Stage 1: tokenizer** (`ConfigParser::tokenize`) walks the file one character at a time:

| Input | Token |
|---|---|
| `{` `}` `;` | `T_LBRACE`, `T_RBRACE`, `T_SEMICOLON` |
| `"some text"` | `T_STRING` (an unclosed quote throws) |
| `# ...` to end of line | `T_COMMENT` |
| anything else up to whitespace | `T_WORD` |
| end of file | `T_EOF` |

**Stage 2: recursive-descent parser** over a `TokenStream` (`peek()`, `advance()`, `expect(type)`; `expect` throws `Wrong syntax of config file - expected X, got Y`):

```
config    := ("server" server)*
server    := "{" directive* "}"                       parseServer / parseDirective
directive := listen HOST:PORT ; | root X ; | index X ; | client_max_body_size N ;
           | server_name X ; | error_page CODE PATH ; | location /PATH { ... }
location  := root X ; | index X ; | methods GET POST DELETE ; | autoindex on|off ;
           | return 3xx URL ; | upload on|off ; | upload_path X ;
           | cgi_extension .ext /interpreter [.ext2 /interp2 ...] ;
           | client_max_body_size N ;                 parseLocation
```

Validation it does:
- **Ports** 1–65535, **error codes** 400–599, **redirect codes** 300–399. Numbers go through `checkNumber` (`strtol` + end pointer + `errno == ERANGE`; this is the one allowed `errno` use in the parser).
- **Methods** only `GET`, `POST`, `DELETE`. **CGI extensions** must start with `.`. **Location paths** must start with `/`.
- **Once per server block:** `listen`, `root`, `index`, `client_max_body_size`, `server_name` (tracked in a `std::set<std::string> seenDirectives`).
- **Across blocks:** `validateUniqueServers` rejects two blocks with the same host, port **and** `server_name`.
- **Unknown directives** throw.

Result: one `ServerConfig` per block (server-level defaults: host, port, root, index, body limit, error pages) holding a `vector<LocationConfig>` (a plain struct of per-location overrides). `hasClientMaxBodySize` lets one location override the server's body limit; `eval.conf` uses that to give `/post_body` a limit of 100 bytes.

### 3.3 The event loop (`src/network/EventLoop.cpp`)

One `std::vector<struct pollfd> _pollFds` holds **three kinds of fd**:

| Kind | Looked up in | Asks for |
|---|---|---|
| Listener sockets | `_listeners` | `POLLIN` → `accept()` new clients |
| Client sockets | `_clients` | `POLLIN` or `POLLOUT`, decided by the client's state (`_updateClientEvents`) |
| CGI pipes | `_cgiByFd` (+ `_cgiFdToClientFd`, `_cgiByClientFd`) | stdin pipe `POLLOUT`, stdout pipe `POLLIN` |

Each pass of `run()`:
1. `_checkCgiProcesses()`: `waitpid(WNOHANG)` on CGI children; a finished job's response goes to its client (§4).
2. `_startQueuedCgi()`: start CGI jobs that are waiting, at most `MAX_CONCURRENT_CGI = 4` at once.
3. `poll()`: timeout 1000 ms, or 100 ms while CGI jobs run so finished children are noticed quickly.
4. `_removeTimedOutCgi()`: a job older than `CGI_TIMEOUT_SECONDS = 10` is killed, the client gets **504**.
5. `_removeTimedOutClients()`: a client idle for `CLIENT_TIMEOUT_SECONDS = 30` is closed (clients waiting on CGI are exempt; step 4 covers them).
6. Every fd with `revents` is dispatched to `_handleListenerEvent`, `_handleClientEvent` or `_handleJobEvent`. An exception for one fd is caught and `_recoverFromError` cleans up only that client (or answers 500 for a CGI pipe). The server keeps running.

### 3.4 One client: the `ClientConnection` state machine

```
READING_HEADERS → READING_BODY → PROCESSING ─┬→ WRITING → DONE  (socket closed)
                                             └→ WAITING_CGI → (job finished) → WRITING → DONE
```

**Reading** (`readFromSocket`, `_processReadBuffer`):
- One `recv()` of up to 64 KiB per `POLLIN`, appended to `_readBuffer`. The code never assumes the whole request arrived at once.
- No `\r\n\r\n` after 8192 bytes → **431**.
- When the header block is complete it parses **only the headers** first, checks `Content-Length` against the body limit (**413**), and then waits until the body is fully buffered before the full parse. That avoids re-parsing a growing 100 MB buffer on every read.
- The body limit comes from `_getClientMaxBodySize()`: the matched location's `client_max_body_size` if it has one, otherwise the selected server's.
- **Large CGI bodies are spooled to disk.** If the request goes to CGI and is chunked or ≥ 1 MB, the body is written to `/tmp/webserv_cgi_body_*` as it arrives (chunked bodies are de-chunked by a small state machine in `_consumeChunkedCgiData`). That file later becomes the CGI's stdin. This exists for the subject tester's 100 MB POST.

**Writing** (`writeToSocket`):
- Headers sit in `_writeBuffer`, the body in `_writeBody`; one `send()` per `POLLOUT`, the offset advances by what was actually sent.
- After the last byte the state becomes `DONE` and the event loop closes the socket. **No keep-alive:** one request per connection, and every response says `Connection: close` (`HttpResponse::closeConnection` defaults to `true`).
- `recv()`/`send()` returning `<= 0` also means `DONE`; `errno` is never looked at.

### 3.5 Parsing HTTP (`src/http/HttpRequestParser.cpp`, Diana)

Request line → `method`, `target`, `version`; `target` is split into `path` and `query` at `?`, and the path is percent-decoded (control characters are rejected). Then the headers, then the body (by `Content-Length`, or decoded from `Transfer-Encoding: chunked` with overflow checks on the hex sizes and an 8 KiB cap on size lines and trailers).

| Problem | Status |
|---|---|
| Malformed request line or header (no `:`, empty name), target not starting with `/` | 400 |
| HTTP/1.1 without `Host`, or a bad `Host` port | 400 |
| Both `chunked` and `Content-Length`, or a non-numeric `Content-Length` | 400 |
| Method other than GET / POST / DELETE / HEAD | 501 |
| Version other than `HTTP/1.0` or `HTTP/1.1` | 505 |
| Body over the limit | 413 |
| Header block over 8192 bytes | 431 |

### 3.6 Routing and deciding (`Router`, `RequestHandler::handle`, Lada)

- **Server block:** `Router::matchServer` lowercases the `Host` header, strips `:port`, and compares it with each candidate's `server_name`. No match → the first block on that port.
- **Location:** `Router::matchLocation` takes the **longest prefix**, matching on a path-segment boundary (`prefixCheck`): `/upload` matches `/upload/x` but not `/uploads`. No match → **404**.
- **Then, in this order:**
  1. `return 3xx URL;` → redirect with a `Location` header.
  2. Method not in `methods` → **405** with `Allow: ...`. HEAD can never be listed, so HEAD always gets 405.
  3. `DELETE` → `..` in the path 403, missing 404, directory 403, otherwise `std::remove()` and **204**.
  4. Extension listed in `cgi_extension` (`_isCgiRequest`) → build a `CgiRequest` (§4) and return `CGI_REQUIRED`.
  5. `POST` to a location without `upload on` → **403**.
  6. Anything else → Diana's `HttpRequestHandler` with the location's `root` (prefix stripped, like nginx `alias`), `index`, `autoindex` and `upload_path`.

### 3.7 Static files, listings, uploads (`HttpRequestHandler`, Diana)

- **GET:** `..` → 404; `stat()` fails → 404 (or 403 for `EACCES`/`EPERM`). A directory serves its `index` file if it exists, else an HTML listing if `autoindex on` (names HTML-escaped and URL-encoded), else 404. A file is read and gets `Content-Type` from `FileUtils::mimeType`.
- **POST upload:** the raw body is saved as `upload_path/<last URL segment>` → **201**. The name must not contain `/` or `..`. There's no multipart parsing; the body is stored as sent.
- Regular disk files are read and written directly, not through `poll()`. The subject explicitly allows this.

### 3.8 Responding and error pages

`ClientConnection::prepareResponse()` turns an `HttpResponse` into bytes with `HttpResponseBuilder` (status line, headers, `Content-Length`, `Connection: close`, default `Content-Type: text/plain`; a 204 gets no `Content-Length`), strips the body for HEAD, and switches to `CLIENT_WRITING`.

Every 4xx/5xx passes through `ErrorResponseFactory`. The body is chosen in this order:
1. The `error_page CODE PATH` from the selected server block, if it's a regular file (`applyCustomPage`).
2. `<root>/errors/<code>.html`.
3. A built-in HTML page.

The status code never changes, only the body.

## 4. The CGI branch (Lada)

CGI means the server runs an external program and sends that program's output back as the response. The key design choice is that **CGI is a job driven by the event loop, not a function call.** A function call would block the whole server while the script runs.

```
RequestHandler::_isCgiRequest   extension in cgi_extension? → script path + interpreter
RequestHandler::_buildCgiRequest → CgiRequest { scriptPath, interpreterPath,
                                               workingDirectory, env[], body / bodyFilePath }
ClientConnection → CLIENT_WAITING_CGI  (socket stays open, nothing read or written on it)
EventLoop::_startCgiForClient (max 4 jobs at once, others wait) → CgiJob::start():
    stdin  = pipe(), or the spooled body file for big bodies (§3.4)
    stdout = pipe();  parent ends set O_NONBLOCK
    fork()
      child:  dup2 → stdin/stdout; close every fd from 3 up to the highest server fd
              (so the script can't hold server sockets); chdir(script dir);
              execve(interpreter, [interpreter, script name], env)
      parent: close the child's ends; register stdout (POLLIN) and stdin (POLLOUT, only if there's a body)
EventLoop::_handleJobEvent:
    POLLOUT → writeToCgi()  one write; when the body is fully sent, close stdin (script sees EOF)
    POLLIN  → readFromCgi() one read; 0 = EOF → close stdout
    closed pipes are removed from poll right away
EventLoop::_checkCgiProcesses (every pass):
    waitpid(WNOHANG); non-zero exit or signal → failed
    done = child exited AND stdout reached EOF  (output still in the pipe isn't lost)
    → CgiJob::buildResponse() → CgiOutputParser → client->prepareOwnedResponse() → job released
EventLoop::_removeTimedOutCgi: job older than 10 s → SIGKILL, 504 to the client
```

**Environment** passed to the script: `GATEWAY_INTERFACE=CGI/1.1`, `SERVER_PROTOCOL`, `REQUEST_METHOD`, `REQUEST_URI`, `QUERY_STRING`, `SCRIPT_NAME`, `SCRIPT_FILENAME`, `PATH_INFO`, `REDIRECT_STATUS=200`, `SERVER_NAME` (the selected block's `server_name`), `SERVER_PORT`, `CONTENT_LENGTH`, `CONTENT_TYPE`, and every other request header as `HTTP_<NAME>` (`User-Agent` → `HTTP_USER_AGENT`).

**Output** (`CgiOutputParser::parse`): header lines (LF or CRLF), a blank line, then the body. `Content-Type` is required. An optional `Status: 404 Not Found` sets the status. The script's own `Content-Length`, `Connection` and `Transfer-Encoding` are dropped because the server does the framing. Bad output → **500**.

Three maps connect pipes to clients:
- `_cgiByFd`: pipe → job
- `_cgiFdToClientFd`: pipe → client
- `_cgiByClientFd`: client → job

If the client disconnects mid-job, `_removeClient` → `_releaseCgi` closes the pipes and kills the child, so there's no dangling pointer. A killed child that isn't reapable yet is parked in `_retiredCgiJobs` and reaped on a later pass, so no zombie is left.

**On purpose:** a CGI target that doesn't exist on disk is still handed to the interpreter, because the subject's `cgi_tester` POSTs to a `.bla` file that doesn't exist and expects the CGI to answer (comment in `RequestHandler::handle`). For `.py` that means a missing script ends as **500** (Python exits with an error), not 404.

**Why you can't write the whole body first and then read the output:** a pipe's buffer only holds about 64 KB. If the script's output fills the stdout pipe while the server is still blocked writing to stdin, each side waits on the other forever. That's a deadlock. Polling both pipes independently avoids it, and big bodies skip the stdin pipe completely by using the spooled file.

## 5. The technology topics, briefly

| Topic | What to know |
|---|---|
| **TCP sockets** | `socket` → `bind` → `listen` → `accept`. Each `accept` returns a new fd for one client. |
| **Non-blocking I/O + `poll()`** | `O_NONBLOCK` makes a call return immediately instead of waiting. `poll()` reports which fds are ready. The subject forbids looking at `errno` after `read`/`write`, which is why the code treats `bytes <= 0` as "done or disconnected". |
| **State machine** | Each connection is in exactly one state (`READING_HEADERS`, `READING_BODY`, `PROCESSING`, `WAITING_CGI`, `WRITING`, `DONE`). The state decides which poll events the fd asks for. |
| **HTTP/1.1 format** | A request line, then headers, then an empty line (`\r\n\r\n`), then an optional body. The body length comes from `Content-Length` or from `Transfer-Encoding: chunked` (hex size, data, repeat, `0` ends it). A response is a status line, headers and a body. |
| **Status codes** | 200/201/204 success · 301/302 redirect · 400 bad syntax · 403 forbidden · 404 missing · 405 method not allowed · 413 body too large · 431 headers too large · 500 server/CGI error · 501 unknown method · 504 CGI timeout · 505 bad version |
| **Config parsing** | Two stages. The tokenizer splits the text into words, strings, `{`, `}` and `;`. A recursive-descent parser (`parseServer`, `parseDirective`, `parseLocation`) walks the tokens with a `TokenStream`, turns them into objects and validates them. |
| **Virtual hosts** | Several server blocks can share one port. The `Host:` header picks between them by `server_name`, and the first block is the default. |
| **Location matching and root** | Longest prefix wins, then the URL is mapped to a path on disk using that location's `root` (prefix stripped). Watch out for `..` path traversal. |
| **CGI** | `fork`/`execve`/`pipe`/`dup2`. Request data goes to the script as environment variables and the body goes to its stdin. The script prints headers without a status line, and the server adds one. |
| **Processes** | You need `waitpid` or you get zombies, and `kill` on timeout. `fork` is only used for CGI. |
| **MIME types** | File extension → `Content-Type`, so the browser knows how to render the response. |
| **Keep-alive** | HTTP/1.1 reuses connections by default. This project closes after every response and says so with `Connection: close`, which is allowed and simpler. |

## 6. Where the docs and the code differ (as of 2026-10-05, `eval-fix`)

- **Class names:** the planning docs mention `CgiHandler`, `ResourceResolver`, `MimeTypes`, `PathUtils`, `ConfigParserBlocks`, `ConfigParserUtils`, `EffectiveRouteConfig` and `SignalState`. None of these exist. The code uses `CgiJob`, `CgiOutputParser`, `HttpRequestHandler`, `ErrorResponseFactory`, `Router`, `FileUtils`, and a single `ConfigParser.cpp` plus `TokenStream.cpp`.
- **Branches:** `eval-fix` is 5 commits ahead of `main`, and `main` has one commit (`05f034b`, the subject tester files) that `eval-fix` doesn't have. Agree as a team which branch you defend from.
- **Fixed since the 2026-09-28 review on `main`:** Makefile dependency tracking, CGI environment (`SERVER_NAME`/`SERVER_PORT`), CGI timeout lowered to 10 s, duplicate `host:port` + `server_name` rejected, `HTTP/1.0` accepted, per-location body limit, large CGI bodies, HEAD handling, `realpath`/`FD_CLOEXEC` replaced (forbidden-function cleanup).
- **Still open:** see §8.9.

## 7. Suggested reading order

1. `docs/READ_THIS.md`: the concepts
2. `docs/Request_lifecycle.md`
3. `docs/lada/CGI_event_loop_design.md`
4. `docs/martin:P/Martin.md`: lists what the network layer expects from the config/CGI side

Then read the code in the order of section 3, starting at `src/main.cpp`.

---

## 8. Evaluation sheet: questions and answers

These are the checks from the official webserv evaluation scale, each followed by the answer for **`eval-fix` @ `31cfc37`**, re-checked on 2026-10-05. Each question points (**→ §**) to the explanation above.

How it was checked on 2026-10-05: `make` after a build (no relink), then curl and `nc` against `conf/default.conf`, temporary configs (comments, duplicate blocks, two server names) and temporary CGI scripts that crash, print no headers, or loop forever. Siege and valgrind were **not** re-run on `eval-fix`; the logs in `docs/Eval/results/final/` are from `main` @ `af2f012`.

Evidence tags:
- **Verified live:** tested against the running server on `eval-fix` on 2026-10-05.
- **Verified on main:** tested on `main` @ `af2f012` on 2026-09-28; the code path is unchanged or only code-read on `eval-fix`.
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

**Q: Explain the basics of an HTTP server.** **[Team]** → §2, §3
A client opens a TCP connection and sends a text request (a request line, headers, a blank line and an optional body). The server parses it, decides what to do based on its config (serve a file, run CGI, upload, delete, redirect, error), and sends back a response (a status line, headers, a blank line and a body). Our server does this for many clients at once, non-blocking, with one `poll()` loop. **Ready**

**Q: Which function did you use for I/O multiplexing?** **[Martin]** → §3.3
`poll()`, in `EventLoop::run()` (`src/network/EventLoop.cpp`). **Ready** (code-read)

**Q: How does `select()` (or the equivalent) work?** **[Martin]** → §3.3
We hand `poll()` an array of `struct pollfd`, one per fd, each with the events we care about (`POLLIN` for "ready to read", `POLLOUT` for "ready to write"). The kernel puts the process to sleep until at least one fd is ready or the timeout runs out (1000 ms, or 100 ms while CGI jobs run). `poll()` then fills in `revents` for every ready fd, and we loop over those and dispatch each one. **Ready** (code-read)

**Q: Do you use only one `poll()`, and how does the server accept clients and read/write them?** **[Martin]** → §3.3, §3.4
Yes, one `poll()` in the main loop. Listener sockets, client sockets and CGI pipes are all in the same `_pollFds` vector. Dispatch goes by fd type:
- **Listener with `POLLIN`:** `accept()` → a new `ClientConnection`, registered for `POLLIN`.
- **Client:** `readFromSocket()` on `POLLIN`, `writeToSocket()` on `POLLOUT`.
- **CGI pipe:** `writeToCgi()` / `readFromCgi()` in `_handleJobEvent`.

`_updateClientEvents()` changes which events a client asks for based on its state. **Ready** (code-read)

**Q: Does `poll()` check read and write AT THE SAME TIME?** (Otherwise the grade is 0.) **[Martin]** → §3.3
Yes. One `poll()` call watches every fd; reading clients and CGI stdout pipes ask for `POLLIN`, writing clients and CGI stdin pipes ask for `POLLOUT`, so reads and writes are multiplexed in the same call. **Ready** (code-read)

**Q: Is there only one read or one write per client per `poll()`? Show the code from `poll()` to the client read/write.** **[Martin]** → §3.4
Walk through `EventLoop::run()` → `_handleClientEvent()` → `ClientConnection::readFromSocket()` (one `recv()`) or `writeToSocket()` (one `send()`). A client asks for `POLLIN` *or* `POLLOUT` depending on its state, never both. The same rule holds for CGI pipes: one `write` per `POLLOUT`, one `read` per `POLLIN`. **Ready** (code-read)
- **Risk:** `_handleListenerEvent` still calls `accept()` in a loop until it returns -1, so later `accept()` calls happen without a fresh readiness report. Some evaluators flag this; one `accept()` per `POLLIN` is the strictly safe version.

**Q: If read/recv/write/send on a socket returns an error, is the client removed?** **[Martin]** → §3.4
Yes. `recv()`/`send()` returning `<= 0` sets the state to `CLIENT_DONE`, and `_handleClientEvent` then calls `_removeClient()`: remove from poll, close the fd, release any CGI job, delete the object. `POLLERR`/`POLLHUP`/`POLLNVAL` also remove the client. **Ready** (code-read)

**Q: Is the return value checked properly (both -1 and 0, not just one)?** **[Lada + Martin]** (Lada: the CGI pipe reads/writes in `CgiJob`; Martin: client sockets) → §3.4, §4
- **Client sockets:** `bytes <= 0` covers both.
- **`CgiJob::writeToCgi()`:** `> 0` advances; 0 or -1 fails the job.
- **`CgiJob::readFromCgi()`:** `> 0` appends, `0` is EOF (close stdout), `-1` fails the job.

**Ready** (code-read)

**Q: Is `errno` checked after read/recv/write/send?** (If yes, the grade is 0.) **[Lada + Martin]** → §3.2, §4
**No.** `grep -rn errno src` shows these uses, none after socket or pipe I/O. Be ready to explain each:
- `ConfigParser.cpp`: `errno == ERANGE` after `strtol()` in `checkNumber`.
- `CgiJob::updateProcessState()`: after `waitpid()` (comment says so).
- `HttpRequestHandler::_handleGET` and `FileUtils::read`: after `stat()`/`open()` on disk files, to choose 403 vs 404.

**Ready** (code-read)

**Q: Is ANY fd read or written without going through `poll()`?** **[Martin + Lada + Diana]** → §3.7, §3.4
Sockets and CGI pipes always go through `poll()`. Regular disk files are read and written directly: `FileUtils::read`/`write` for pages and uploads, `opendir`/`readdir` for autoindex, and the spooled CGI body file in `/tmp`. The subject explicitly says regular disk files don't need `poll()`, so have that sentence ready. **Ready** (defend it)

**Q: Does it compile without relinking?** **[Team]**
Verified live: a second `make` prints "Nothing to be done for 'all'". Flags are `-Wall -Wextra -Werror -std=c++98`. The Makefile now has `-include $(DEPS)` (built with `-MMD -MP`), so editing a header rebuilds the objects that use it. **Ready**

### 8.2 Configuration

**Q: Are the HTTP status codes correct?** **[Lada + Diana]** → §3.5, §3.6, §4

| Situation | Code | Evidence |
|---|---|---|
| GET a file / listing | 200 | verified live |
| Upload | 201 | verified live |
| DELETE | 204, no body, no `Content-Length` | verified live |
| Redirect (`/old`) | 301 + `Location: /` | verified live |
| Missing file | 404 | verified live |
| Method not allowed | 405 + `Allow: GET` | verified live |
| Body too large (2 MB vs 1 MB) | 413 | verified live |
| Unknown method (`FOO`) | 501 | verified live |
| `HTTP/1.0` request | 200 | verified live |
| HEAD | 405 | verified live |
| CGI crash / no `Content-Type` | 500 | verified live |
| CGI infinite loop | 504 | verified live |

**Risk:**
- A `.py` CGI script that doesn't exist gives **500**, not 404. This is deliberate (§4, subject tester), so explain it rather than call it a bug.
- HEAD always gets 405 (§3.6). nginx would answer it like GET without a body.
- A directory with no index and `autoindex off` gives 404 (nginx uses 403).

**Q: Multiple servers with different ports.** **[Lada + Martin]** → §3.1, §3.2
`conf/multi_port.conf` has 8000 → `www/site1` and 8001 → `www/site2`. `WebServer::_createListeners` creates one listener per `host:port`. **Ready** (verified on main)

**Q: Multiple servers with different hostnames** (`curl --resolve example.com:80:127.0.0.1 http://example.com/`). **[Lada]** → §3.6
`Router::matchServer` compares the `Host` header (with `:port` stripped, case-insensitive) against every block's `server_name`, and falls back to the first block on that port. The selected server is then used for routing, the body limit, error pages and `SERVER_NAME` in CGI. **Ready** (verified on main: `alpha.test` → site1, `beta.test` → site2 on one port)
- `server_name` accepts exactly **one** name: `server_name a b;` is rejected (verified live). Use two blocks if you need two names.

**Q: Default error page (try changing the 404).** **[Lada + Diana]** → §3.8
`error_page <code> <path>` is parsed into the server block; `ErrorResponseFactory::applyCustomPage` serves that file for every 4xx/5xx (static, parser and CGI errors), keeping the status code. Fallbacks: `<root>/errors/<code>.html`, then a built-in page. **Ready** (verified on main; `GET /nope` → 404 with the custom page verified live)

**Q: Limit the client body** (`curl -X POST -H "Content-Type: plain/text" --data "..."`). **[Lada + Martin]** → §3.2, §3.4
`client_max_body_size` can be set per server **and per location**; the location value wins. `Content-Length` is checked as soon as the headers are complete, chunked bodies while they're decoded. Verified live: a 2 MB upload against the 1 MB default → 413. For the sheet's exact curl command, put `client_max_body_size 10;` in a location and POST more than 10 bytes. **Ready**

**Q: Routes in a server pointing to different directories.** **[Lada + Diana]** → §3.6
`RequestHandler` uses the matched location's `root` and strips the location prefix (like nginx `alias`), so `location /uploads { root ./www/site1/uploads; }` maps `/uploads/f.txt` → `./www/site1/uploads/f.txt`. **Ready** (verified live; be ready to explain that the prefix is stripped, which differs from nginx `root`)

**Q: A default file to serve when a directory is requested.** **[Lada + Diana]** → §3.7
The location's `index` is used, falling back to the server's `index`. **Ready** (verified on main: `GET /listing/` serves `listing/index.html`)

**Q: A list of accepted methods for a route** (e.g. delete something with and without permission). **[Lada]** → §3.6
`RequestHandler::handle` checks `location->methods` and returns 405 with `Allow`. Verified live:
- `DELETE /index.html` (location allows only `GET`) → **405**, `Allow: GET`.
- `DELETE /uploads/<file>` (location allows `GET DELETE`) → **204**; again → **404**.

POST also needs `upload on` in the location, otherwise 403. **Ready**

### 8.3 Basic checks

**Q: GET, POST and DELETE requests work.** **[Lada + Diana]** (Lada: DELETE; Diana: GET and POST) → §3.6, §3.7
Verified live:
- **GET:** static files, index files, autoindex. **Ready**
- **POST:** `POST /upload/zz_eval.txt` → 201, saved into the location's `upload_path`. **Ready**
  - The file name is the last URL segment, and the raw body is stored (no multipart parsing). `POST /upload` with no name saves a file called `upload`.
- **DELETE:** existing file → 204; again → 404; directory or `..` → 403; not allowed → 405. **Ready**

**Q: UNKNOWN requests do not crash the server.** **[Diana + Martin]** → §3.5, §3.3
Verified live: `FOO /` → 501 and the server keeps running. Malformed lines/headers or a bad `Host` → 400, wrong version → 505, oversized headers → 431. `SIGPIPE` is ignored, and an exception while handling one fd is caught in `EventLoop::run()` so only that client is affected. **Ready**

**Q: Every test returns the appropriate status code.** **[Lada + Diana]** → §3.5, §3.6
See the table in 8.2. The only codes worth explaining are the deliberate ones: missing CGI script → 500 and HEAD → 405. **Ready** (with explanation)

**Q: Upload a file to the server and get it back.** **[Diana]** → §3.7
Verified live with `conf/default.conf`: `POST /upload/zz_eval.txt` (201) → `GET /uploads/zz_eval.txt` returns the content → `DELETE /uploads/zz_eval.txt` (204) → `DELETE` again gives 404. **Ready**

### 8.4 Check CGI

**Q: The server works fine with CGI.** **[Lada]** → §4
Verified live: `GET /cgi-bin/env.py` and `POST /cgi-bin/echo.py` → 200. Scripts run as non-blocking `CgiJob`s whose pipes are polled by the same event loop; at most 4 run at once and the rest wait their turn. **Ready**

**Q: CGI runs in the correct directory for relative file access.** **[Lada]** → §4
Yes. `workingDirectory` is the script's directory; the child calls `chdir()` into it before `execve()` and passes the bare script name as `argv[1]`. If `chdir` fails, the child exits and the client gets 500. **Ready** (code-read)

**Q: Test CGI with GET and POST.** **[Lada]** → §4
Verified live:
- **GET:** `REQUEST_METHOD` and `QUERY_STRING` reach the script; `env.py` shows `SERVER_NAME=site1`, `SERVER_PORT=8001`.
- **POST:** `--data 'a=1'` reaches the script's stdin (echoed back), with `CONTENT_LENGTH`/`CONTENT_TYPE` set. Small bodies go through the stdin pipe one write per `POLLOUT`; bodies ≥ 1 MB or chunked are spooled to a temp file that becomes stdin (§3.4).

Demo:
```
curl "http://127.0.0.1:8001/cgi-bin/hello.py?name=x"
curl -X POST --data "a=1" http://127.0.0.1:8001/cgi-bin/echo.py
curl http://127.0.0.1:8001/cgi-bin/env.py
```
**Ready**

**Q: Scripts with errors or infinite loops are handled; the server never crashes and an error is visible.** **[Lada]** → §4
Verified live:
- **Script exits with an error (`sys.exit(3)`):** 500. **Ready**
- **Output without `Content-Type`:** 500. **Ready**
- **Infinite loop (`while True: pass`):** **504** after about 10 s. A static request during the loop was answered in ~1 ms, and afterwards the server had no child processes left (no zombie). **Ready**
- **Script that doesn't exist:** 500 (deliberate, see §4).

### 8.5 Check with a browser

**Q: Look at the request and response headers in the browser's network tab.** **[Diana]** → §3.8
Every response has `Content-Length`, `Content-Type` and `Connection: close` (verified live; the server really does close after every response, so the header is honest). 405 adds `Allow`, redirects carry `Location`, 204 has no body and no `Content-Length`. **Ready**

**Q: It can serve a fully static website.** **[Diana]** → §3.7
`www/site1` and `www/site2` are full demo sites with CSS and images. MIME types come from `FileUtils::mimeType`. **Ready** (open them in the browser before the defence)

**Q: Try a wrong URL.** **[Diana]** → §3.8
Verified live: 404 with the custom page from `error_page`. **Ready**

**Q: Try to list a directory.** **[Lada + Diana]** (Lada: passing the location's `autoindex` flag on; Diana: generating the listing) → §3.7
Verified live: `GET /listing/` → 200. A directory without an index under `autoindex off` → 404. **Ready**

**Q: Try a redirected URL.** **[Lada]** → §3.6
Verified live: `GET /old` → `301 Moved Permanently` with `Location: /`. The parser accepts any code from 300 to 399 for `return`. **Ready**

### 8.6 Port issues

**Q: Multiple ports with different websites; the browser shows the right site.** **[Lada + Martin]** → §3.1
Use `conf/multi_port.conf`: `127.0.0.1:8000` → site1, `127.0.0.1:8001` → site2. **Ready**

**Q: Set up the same port multiple times in the config. It should not work.** **[Lada + Martin]** → §3.1, §3.2
Verified live: two blocks with the same `host:port` **and** the same `server_name` → `webserv: duplicate server block for 127.0.0.1:8124 (server_name a)`, exit 1. **Ready**
- Two blocks on one port with **different** `server_name`s are allowed on purpose: that's virtual hosting, one listener with two candidates. Explain the grouping in `_createListeners`.
- A second `listen` inside one block is also rejected (once-per-block rule).

**Q: Run multiple servers at the same time with different configs but common ports.** **[Martin]** → §3.1
The second process fails at `bind()` (`webserv: bind() failed`, exit 1) while the first keeps working. A port can only be bound by one listening socket at a time. **Ready** (verified on main)

### 8.7 Siege & stress test

**Q: Availability above 99.5% for `siege -b` on an empty page.** **[Martin]** → §3.3
`docs/Eval/results/final/audit-siege-*.log` show **100.00%** availability, but those runs were on `main` @ `af2f012`. **Re-run `siege -b` on `eval-fix` before the defence.**

**Q: No memory leaks (memory usage doesn't keep growing).** **[Team]** (Martin: client objects and fds; Lada: `CgiJob`s and CGI pipes) → §3.4, §4
`docs/Eval/results/final/valgrind.log` (from `main`): 0 errors, 0 bytes definitely lost. "Still reachable" memory at Ctrl+C comes from the default `SIGINT` action ending the process (there's no graceful shutdown). That isn't a leak; say so if asked. **Ready** (re-run valgrind on `eval-fix`)

**Q: No hanging connections.** **[Lada + Martin]** → §3.3, §4
Every connection closes after its response, idle clients are removed after 30 s, and clients waiting on CGI are answered with 504 after 10 s (verified live). **Ready**

**Q: Siege can run indefinitely without restarting the server.** **[Martin]** → §3.3
The endurance logs in `docs/Eval/results/final/` (from `main`) show a flat fd count and memory. **Ready** (confirm with a long `siege -b` run on `eval-fix`)

### 8.8 Bonus

**Q: Working cookies and session system.** **[Team]**
**Missing.** Nothing in `src/` or `include/` handles cookies or sessions.

**Q: More than one CGI system.** **[Lada]** → §3.2, §4
The parser accepts several extension/interpreter pairs (`cgi_extension .py /usr/bin/python3 .sh /bin/sh;`), and `LocationConfig::cgiExtensions` maps extension → interpreter. Two are configured: `.py` → `python3` (`default.conf`) and `.bla` → `cgi_tester` (`eval.conf`). **Ready** if you demo both; a `.sh` demo script would make it obvious.

### 8.9 Before the defence: fixes, most important first

1. **[Lada]** **Comments break the config.** The tokenizer creates `T_COMMENT` tokens but the parser never skips them, so any `# comment` line gives `Wrong syntax of config file - expected word, got: "" (comment)` (verified live). Evaluators often add comments when editing the config. Either skip comments in the tokenizer or in `TokenStream`.
2. **[Team]** Decide which branch you defend from (`eval-fix` vs `main`, §6), then re-run siege and valgrind on it.
3. **[Lada]** Prepare the explanation for missing CGI script → 500 (§4); optionally return 404 for a missing script only when the interpreter isn't `cgi_tester`.
4. **[Lada + Diana]** Decide on HEAD → 405. Allowing HEAD wherever GET is allowed would match nginx.
5. **[Martin]** Optional: one `accept()` per `POLLIN`; handle `SIGINT` for a clean shutdown (clean valgrind output).
6. **[Lada]** Optional: let `server_name` take several names (`addServerName` already exists in `ServerConfig`).
7. **[Lada]** Bonus: add a `.sh` CGI demo script.
