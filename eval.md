# Webserv — Evaluation Preparation

Every command in this file was run on this project, and the "Expected" output is what really happened.
Run all commands from the **repository root**.

---

## 0. The project in 2 minutes (what to say first)

**What is it?**
Webserv is our own HTTP server, written in C++98 (like a very small NGINX).
A browser or `curl` connects to it, sends an HTTP request (`GET /index.html HTTP/1.1` + headers + optional body),
and our server sends back an HTTP response (`HTTP/1.1 200 OK` + headers + body).

**What must it do?** (from the subject)
- Read a **configuration file** (NGINX-like): ports, roots, error pages, body size limit, routes.
- Serve a **static website** (HTML, CSS, images), **upload** files (POST), **delete** files (DELETE).
- Run **CGI** scripts (we use Python) based on the file extension.
- Listen on **multiple ports** at the same time.
- Be **non-blocking**: only **one `poll()`** for all sockets and pipes. It must never hang and never crash.

**How does it work? (the life of one request)**

```
main.cpp            -> reads the config path (default: conf/default.conf)
ConfigParser        -> tokenizes + parses the config into ServerConfig / LocationConfig
WebServer           -> creates one Listener (socket + bind + listen) per unique host:port
EventLoop::run()    -> the ONE poll() loop. Everything happens here:
   listener ready   -> accept() new client           -> ClientConnection created
   client POLLIN    -> recv() a chunk                 -> HttpRequestParser builds HttpRequest
   request complete -> Router picks server + location -> RequestHandler decides:
                          static GET / POST upload / DELETE / redirect / autoindex / error
                          or CGI -> fork + execve, pipes are added to the same poll()
   client POLLOUT   -> send() a chunk of the response -> when all sent: close + remove client
```

| Part | Files |
|---|---|
| Entry point | [src/main.cpp](src/main.cpp) |
| Config parsing | [src/config/](src/config/) (`TokenStream`, `ConfigParser`, `ServerConfig`) |
| Sockets + poll loop | [src/network/](src/network/) (`Listener`, `EventLoop`, `ClientConnection`) |
| HTTP parsing + responses | [src/http/](src/http/) (`HttpRequestParser`, `HttpRequestHandler`, `HttpResponseBuilder`) |
| Choosing server/location | [src/routing/Router.cpp](src/routing/Router.cpp) |
| Decision static vs CGI, error pages | [src/core/](src/core/) (`RequestHandler`, `ErrorResponseFactory`) |
| CGI | [src/cgi/](src/cgi/) (`CgiJob`, `CgiRequest`, `CgiOutputParser`) |

---

## 1. Before starting (setup)

```bash
git clone <repo> webserv_eval && cd webserv_eval   # in an EMPTY folder
make                 # compiles with c++ -Wall -Wextra -Werror -std=c++98
make                 # 2nd time: "Nothing to be done for 'all'"  -> no relink
make re              # rules: all, clean, fclean, re, $(NAME)
sudo apt-get install siege
```

Configs we prepared in [conf/](conf/):

| File | Port(s) | Shows |
|---|---|---|
| `default.conf` | 8001 | everything: static site, listing, redirect, upload, delete, CGI, error pages |
| `multi_port.conf` | 8000 + 8001 | two different websites on two ports |
| `small_body_limit.conf` | 8000 | `client_max_body_size 10` -> 413 |
| `upload_delete.conf` | 8000 | upload + delete only |
| `cgi.conf` | 8000 | CGI only |
| `static_only.conf` | 8000 | static only |
| `eval.conf` | 8080 | the config for the subject `tester` |

---

## 2. README check

```bash
head -1 README.md
grep -n "^## " README.md
```
Expected: the first line is `*This project has been created as part of the 42 curriculum by mjusta, lformank, dkolarov.*`
and the sections `Description`, `Instructions`, `Resources` (with `### AI use`) exist.

---

## 3. "Check the code and ask questions"

### 3.1 Basics of an HTTP server
- HTTP is a text protocol on top of TCP. The client sends a **request**: request line (`METHOD /path HTTP/1.1`),
  headers (`Host:`, `Content-Length:` ...), an empty line `\r\n\r\n`, then an optional body.
- The server answers with a **response**: status line (`HTTP/1.1 404 Not Found`), headers
  (`Content-Type`, `Content-Length`), empty line, body.
- The server's job is to: open a socket, `bind` it to IP:port, `listen`, `accept` clients, read the request,
  find the resource, and send the response with the **correct status code**.
- We answer every response with `Connection: close`. After the response is sent, we close the socket (no keep-alive).

### 3.2 Which event mechanism? -> `poll()`
There is **one** `poll()` call in the whole project: [EventLoop.cpp:397](src/network/EventLoop.cpp#L397), inside `EventLoop::run()` ([EventLoop.cpp:384](src/network/EventLoop.cpp#L384)).

```bash
grep -rn "poll(" src/        # only one real call
```

### 3.3 How does poll() work?
- We give `poll()` an array of `struct pollfd` (`fd`, `events` we want, `revents` that happened).
- `poll()` sleeps until at least one fd is ready (or the timeout: 1000 ms, or 100 ms while CGI is running).
- It then fills `revents`: `POLLIN` = can read without blocking, `POLLOUT` = can write without blocking,
  `POLLHUP/POLLERR` = closed/error.
- We loop over the array and do **one** read or write only on the fds that are ready.

### 3.4 Only one poll, and how accept / read / write are handled
The `pollfd` array contains 3 kinds of fds:
1. **Listener sockets** (`POLLIN`) -> `_handleListenerEvent` [EventLoop.cpp:152](src/network/EventLoop.cpp#L152) -> `accept()` in [Listener.cpp:108](src/network/Listener.cpp#L108) -> new client added with `POLLIN`.
2. **Client sockets** -> `_handleClientEvent` [EventLoop.cpp:170](src/network/EventLoop.cpp#L170):
   - `POLLIN` -> `readFromSocket()` [ClientConnection.cpp:310](src/network/ClientConnection.cpp#L310) (one `recv`)
   - `POLLOUT` -> `writeToSocket()` [ClientConnection.cpp:365](src/network/ClientConnection.cpp#L365) (one `send`)
3. **CGI pipes** -> `_handleJobEvent` [EventLoop.cpp:270](src/network/EventLoop.cpp#L270):
   - stdin pipe `POLLOUT` -> `writeToCgi()` [CgiJob.cpp:224](src/cgi/CgiJob.cpp#L224)
   - stdout pipe `POLLIN` -> `readFromCgi()` [CgiJob.cpp:244](src/cgi/CgiJob.cpp#L244)

**Reading and writing at the same time:** `_updateClientEvents` [EventLoop.cpp:129](src/network/EventLoop.cpp#L129) sets `POLLIN` while the client
still sends the request and `POLLOUT` when a response is ready. Both are watched by the same `poll()`.

**Code path to show:** `run()` -> `poll()` -> loop over `readyFds` -> `_isListenerFd / _isClientFd / _isCgiFd` -> handler -> `recv/send/read/write`.

All sockets and pipes are non-blocking: `fcntl(fd, F_SETFL, O_NONBLOCK)` in [Listener.cpp:38](src/network/Listener.cpp#L38),
[EventLoop.cpp:24](src/network/EventLoop.cpp#L24) and [CgiJob.cpp:98](src/cgi/CgiJob.cpp#L98).

### 3.5 Return values of read/recv/write/send, and removal of the client

```bash
grep -rnE "\b(recv|send|read|write)\(" src/ | grep -v FileUtils::
```

| Call | Where | `> 0` | `0` | `-1` |
|---|---|---|---|---|
| `recv` | ClientConnection.cpp:316 | append data | client closed -> `CLIENT_DONE` | error -> `CLIENT_DONE` |
| `send` | ClientConnection.cpp:371, 389 | advance offset | -> `CLIENT_DONE` | -> `CLIENT_DONE` |
| `write` (CGI stdin) | CgiJob.cpp:228 | advance offset | job failed, pipe closed | job failed, pipe closed |
| `read` (CGI stdout) | CgiJob.cpp:248 | append output | EOF -> close pipe | job failed -> 500 |

We check `<= 0`, so both `0` and `-1` are handled. When a client is `CLIENT_DONE`, the event loop calls
`_removeClient()` [EventLoop.cpp:114](src/network/EventLoop.cpp#L114): it removes the fd from poll, deletes the object and closes the socket.
`POLLERR/POLLHUP/POLLNVAL` on a client also removes it ([EventLoop.cpp:179](src/network/EventLoop.cpp#L179)).

### 3.6 errno
```bash
grep -rn "errno" src/
```
`errno` is **never** checked after `recv/send/read/write`. It is only used after:
- `stat()`/`open()` on files, to choose 403 or 404 ([FileUtils.cpp:39](src/utils/FileUtils.cpp#L39), [HttpRequestHandler.cpp:76](src/http/HttpRequestHandler.cpp#L76))
- `strtol()` in the config parser (`ERANGE`)
- `waitpid()` for CGI (`ECHILD`)

### 3.7 Regular disk files
Regular files (config, static pages, uploads, error pages) are read and written directly with `open/read/write` in
[FileUtils.cpp:61](src/utils/FileUtils.cpp#L61) without poll. **The subject allows this.** They are local and small, so they don't stall the loop.
We even refuse non-regular files (a FIFO cannot block us: `O_NONBLOCK` + `S_ISREG` check).

### 3.8 Allowed functions only
```bash
nm -uC webserv | grep -v -E 'std::|operator|__cxa|__gxx|_Unwind|__cxx'
```
You will only see allowed syscalls (`socket, bind, listen, accept, recv, send, poll, fork, execve, pipe, dup2, chdir,
waitpid, kill, fcntl, open, read, write, close, stat, opendir, readdir, closedir, getaddrinfo, ...`) plus C/C++ standard
library functions (`strtol`, `time`, `remove`, ...). `fork()` is used **only** for CGI ([CgiJob.cpp:124](src/cgi/CgiJob.cpp#L124)).

---

## 4. Configuration

Start: `./webserv conf/default.conf` (port 8001). Use a second terminal for the commands.

### 4.1 Multiple websites on different interfaces and ports
```bash
./webserv conf/multi_port.conf
curl -s http://127.0.0.1:8000/ | grep "<title>"     # site1
curl -s http://127.0.0.1:8001/ | grep "<title>"     # site2 (different content)
```
For a **different interface**, change the second server in the config to `listen 127.0.0.2:8000;`
(all of `127.x.x.x` is loopback on Linux):
```bash
curl -s http://127.0.0.1:8000/ | grep "<title>"     # site1
curl -s http://127.0.0.2:8000/ | grep "<title>"     # site2
```
How it works: [WebServer.cpp:24](src/core/WebServer.cpp#L24) creates **one Listener per unique host:port**. Each Listener has
its own socket in the same poll.

### 4.2 Default error page (and modifying the 404)
```bash
curl -i http://127.0.0.1:8001/does_not_exist      # 404 + our custom page (www/site1/errors/404.html)
```
To modify it, edit `www/site1/errors/404.html`, or point `error_page 404 <other file>;` in the config to another file, then restart
the server and run the same curl again.
If no `error_page` is configured, or the file is missing, we generate a **default page** ourselves:
```bash
curl -i -X DELETE http://127.0.0.1:8001/index.html   # 405 -> built-in page "<h1>405 Method Not Allowed</h1>"
```
Code: [ErrorResponseFactory.cpp](src/core/ErrorResponseFactory.cpp)

### 4.3 Limit client body size
```bash
./webserv conf/small_body_limit.conf              # client_max_body_size 10
curl -i -X POST -H "Content-Type: plain/text" --data "short" http://127.0.0.1:8000/upload/b.txt
#   -> 201 Created  (5 bytes)
curl -i -X POST -H "Content-Type: plain/text" --data "BODY IS HERE write something longer" http://127.0.0.1:8000/upload/b.txt
#   -> 413 Payload Too Large
rm -f www/site1/uploads/b.txt
```
The limit can be set per server and overridden per location (see `/post_body` in `eval.conf`, which uses 100).
It is checked while reading, using `Content-Length` or the size of the un-chunked body ([ClientConnection.cpp:581](src/network/ClientConnection.cpp#L581)).

### 4.4 Routes to different directories
In `default.conf`, `/listing` has `root ./www/site1/listing` and `/cgi-bin` has `root ./cgi-bin` (outside `www`!):
```bash
curl -s http://127.0.0.1:8001/listing/banana.html | head -3    # file from www/site1/listing/
curl -s http://127.0.0.1:8001/cgi-bin/hello.py                 # file from ./cgi-bin/
```
The Router picks the **longest matching location** prefix ([Router.cpp:105](src/routing/Router.cpp#L105)).

### 4.5 Default file for a directory (`index`)
```bash
curl -i http://127.0.0.1:8001/          # location / has "index index.html" -> 200
curl -i http://127.0.0.1:8001/upload/   # no index + autoindex off -> 404
```

### 4.6 Accepted methods per route (DELETE with / without permission)
```bash
curl -X POST --data "to delete" http://127.0.0.1:8001/upload/del.txt   # 201, saved in www/site1/uploads/
curl -i -X DELETE http://127.0.0.1:8001/uploads/del.txt   # /uploads allows GET DELETE -> 204 No Content
curl -i -X DELETE http://127.0.0.1:8001/uploads/del.txt   # already gone -> 404
curl -i -X DELETE http://127.0.0.1:8001/index.html        # / allows only GET -> 405 + "Allow: GET"
```

---

## 5. Basic checks (GET, POST, DELETE, UNKNOWN, upload + retrieve)

```bash
./webserv conf/default.conf
# GET
curl -i http://127.0.0.1:8001/                                 # 200
# POST = upload (body is saved as www/site1/uploads/<name>)
curl -i -X POST --data "hello eval" http://127.0.0.1:8001/upload/eval.txt   # 201 Created
curl -i -X POST --data-binary @www/site1/assets/crew-1.png http://127.0.0.1:8001/upload/photo.png
# retrieve the uploads
curl http://127.0.0.1:8001/uploads/eval.txt                    # hello eval
curl -o /tmp/back.png http://127.0.0.1:8001/uploads/photo.png && cmp /tmp/back.png www/site1/assets/crew-1.png && echo SAME
curl http://127.0.0.1:8001/uploads/                            # autoindex lists the files
# DELETE
curl -i -X DELETE http://127.0.0.1:8001/uploads/eval.txt       # 204
curl -i -X DELETE http://127.0.0.1:8001/uploads/photo.png      # 204
# UNKNOWN method -> no crash
curl -i -X BLAH http://127.0.0.1:8001/                         # 501 Not Implemented
```

With **telnet** (type the lines by hand, then press Enter twice):
```
telnet 127.0.0.1 8001
GET /listing/ HTTP/1.1
Host: localhost

```
-> `HTTP/1.1 200 OK` + the directory listing. Typing garbage + Enter twice gives `400 Bad Request`.
If you close telnet in the middle of a request (`Ctrl+]` then `quit`), the server just removes the client.

Status codes we use: 200, 201, 204, 301, 400, 403, 404, 405, 413, 431, 500, 501, 504.

Upload code: `_handlePOST` [HttpRequestHandler.cpp:108](src/http/HttpRequestHandler.cpp#L108). DELETE code: `_handleDELETE` [HttpRequestHandler.cpp:149](src/http/HttpRequestHandler.cpp#L149).

---

## 6. CGI

Config (`default.conf`): `location /cgi-bin { methods GET POST; root ./cgi-bin; cgi_extension .py /usr/bin/python3; }`

**How it works** ([CgiJob.cpp:48](src/cgi/CgiJob.cpp#L48)):
1. `RequestHandler` sees that the file extension `.py` matches `cgi_extension`, so the request becomes a CGI job.
2. We create 2 pipes (stdin and stdout) and then `fork()`.
3. The child does `dup2` of the pipes to stdin/stdout, `chdir()` into the script's directory (this is why relative paths work),
   and `execve("/usr/bin/python3", [python3, script], env)`.
4. The env contains `REQUEST_METHOD, QUERY_STRING, CONTENT_LENGTH, CONTENT_TYPE, SCRIPT_NAME, SCRIPT_FILENAME, PATH_INFO, SERVER_PROTOCOL, GATEWAY_INTERFACE, ...`.
5. The parent adds both pipes to the **same poll()**. It writes the body to the CGI's stdin and reads its stdout until EOF.
6. A chunked body is **un-chunked** before it is sent to the CGI. The CGI sees EOF at the end of the body.
7. The CGI output (headers + body) is parsed into the HTTP response. `waitpid(WNOHANG)` reaps the child, so there are no zombies.
8. There is a 10 s timeout ([EventLoop.hpp:27](include/network/EventLoop.hpp#L27)). After it, we `kill` the child and answer `504`.

```bash
./webserv conf/default.conf
# GET with query string
curl "http://127.0.0.1:8001/cgi-bin/hello.py?name=Diana"
# POST with body
curl -X POST --data "posted body" "http://127.0.0.1:8001/cgi-bin/echo.py?name=post"
# chunked POST (un-chunked by the server)
curl -X POST -H "Transfer-Encoding: chunked" --data "chunked body" http://127.0.0.1:8001/cgi-bin/echo.py
# the environment the CGI receives
curl "http://127.0.0.1:8001/cgi-bin/env.py?a=1"
```

**Bad CGI scripts.** Create them for the test, then delete them:
```bash
printf 'while True:\n    pass\n'   > cgi-bin/loop.py     # infinite loop
printf 'import sys\nsys.exit(1)\n' > cgi-bin/crash.py    # exits with an error
printf 'print("broken"\n'          > cgi-bin/syntax.py   # syntax error
printf 'import os\nprint("Content-Type: text/plain")\nprint()\nprint(os.getcwd())\nprint(open("hello.py").readline())\n' > cgi-bin/cwd.py

curl -i http://127.0.0.1:8001/cgi-bin/crash.py    # 500 Internal Server Error
curl -i http://127.0.0.1:8001/cgi-bin/syntax.py   # 500
curl -i http://127.0.0.1:8001/cgi-bin/loop.py     # waits ~10 s -> 504 Gateway Timeout, child killed
curl -i http://127.0.0.1:8001/                    # server still alive (also try it DURING the loop: it answers at once)
curl http://127.0.0.1:8001/cgi-bin/cwd.py         # prints .../cgi-bin and reads hello.py by relative path
ps --ppid $(pgrep -x webserv)                     # no zombie python processes left

rm cgi-bin/loop.py cgi-bin/crash.py cgi-bin/syntax.py cgi-bin/cwd.py
```

---

## 7. Browser

Open `http://127.0.0.1:8001/` in the browser, press F12 and open the **Network** tab.
- `/` -> full static site (HTML + CSS + images). Look at the request headers (`Host`, `User-Agent`...) and the
  response headers (`Content-Type`, `Content-Length`, `Connection: close`).
- `/wrong-url` -> our custom 404 page
- `/listing/` -> directory listing (autoindex), and the links work
- `/uploads/` -> listing of the uploaded files
- `/old` -> `301` + `Location: /` -> the browser goes to `/`
- `/cgi-bin/hello.py?name=browser` -> CGI page

---

## 8. Port issues

**Multiple interfaces/ports with different websites:** see 4.1 and open both URLs in the browser.

**Several servers on the same interface:port.** We implemented **virtual hosts**. The server is chosen by the `Host` header
([Router.cpp:78](src/routing/Router.cpp#L78)), and the first server is the default:
```
server { listen 127.0.0.1:8080; server_name site1; root ./www/site1; location / { methods GET; root ./www/site1; index index.html; } }
server { listen 127.0.0.1:8080; server_name site2; root ./www/site2; location / { methods GET; root ./www/site2; index index.html; } }
```
```bash
curl -s http://127.0.0.1:8080/ | grep "<title>"                 # site1 (default)
curl -s -H "Host: site2" http://127.0.0.1:8080/ | grep "<title>" # site2
```

**Two webserv programs with the same interface:port:**
```bash
./webserv conf/eval.conf          # terminal 1
./webserv conf/eval.conf          # terminal 2 -> "[ERROR] webserv: bind() failed", exit code 1
```
Why: the OS allows only one socket to `bind` the same IP:port. We use `SO_REUSEADDR`, which only allows a restart while
old connections are in `TIME_WAIT`; we do not use `SO_REUSEPORT`. The first server keeps working and nothing crashes.

---

## 9. Siege and stress test

```bash
./webserv conf/default.conf
touch www/site1/empty.html                                   # "empty page" for the test (delete after)
siege -b -c 25 -t 30S http://127.0.0.1:8001/empty.html       # availability must be > 99.5 %
siege -b -c 50 -r 100 http://127.0.0.1:8001/                 # full page
```
While siege runs, in another terminal:
```bash
watch -n1 'ps -o pid,rss,vsz,cmd -p $(pgrep -x webserv)'        # RSS memory must stay stable
watch -n1 'ls /proc/$(pgrep -x webserv)/fd | wc -l'             # fd count goes back to 4 when idle
ss -tanp | grep 8001 | grep ESTAB                               # after siege: no hanging connections
```
Why there are no hanging connections: every response is followed by a close, and idle clients are dropped after 30 s
([EventLoop.hpp:26](include/network/EventLoop.hpp#L26), `_removeTimedOutClients`).
Our earlier runs: siege availability 100 % ([docs/Eval/results/final/](docs/Eval/results/final/)), and the subject tester passed.

**Memory leaks (valgrind):**
```bash
valgrind --leak-check=full ./webserv conf/default.conf
# do some requests in another terminal, then Ctrl+C
```
Result: `definitely lost: 0`, `indirectly lost: 0`, `possibly lost: 0`, `ERROR SUMMARY: 0 errors`.

---

## 10. The subject tester

```bash
./webserv conf/eval.conf
./tester http://127.0.0.1:8080        # press Enter at the prompts
```
All tests pass (about 2 minutes), including 100 MB POSTs to the CGI and the 128-worker load test.
`.bla` files are run with `./cgi_tester`. A big CGI body is saved to a temporary file first, so it is not kept in memory.

---

## 11. Weak spots (know them before the evaluator finds them)

- **A missing CGI script returns 500, not 404** (`curl -i http://127.0.0.1:8001/cgi-bin/nope.py`). Python cannot open the
  file and exits with an error. If they ask, explain this, or add a `stat()` check before starting the CGI.
- **Ctrl+C:** there is no SIGINT handler, so valgrind shows about 100 KB **"still reachable"** (memory not freed at exit, but
  not lost while running). Only "definitely/indirectly/possibly lost" are real leaks, and those are 0.
- **No keep-alive:** we always send `Connection: close`. The subject suggests HTTP/1.0 as the reference, so this is fine.
- In `default.conf`, `error_page` lines for 403/405/413/500 point to files that don't exist. In that case we fall back to
  the built-in default page (this shows the "default error pages" requirement).
- Uploads are the **raw request body** saved under the file name in the URL (`POST /upload/<name>`). There is no multipart
  parsing.
