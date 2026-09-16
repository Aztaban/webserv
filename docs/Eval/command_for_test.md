# Webserv — Full Testing Checklist

For every test below: **run the command → check it matches "Expected Result" → understand "How the Server Handles It"** so you can explain the behavior, not just demonstrate it, during evaluation.

Assumes the server is running with:
```bash
make && ./webserv conf/default.conf
```
listening on `127.0.0.1:8001`, with an upload location at `/upload` (saving to `/uploads`) and a CGI location at `/cgi-bin`. Adjust paths/ports to match your actual config.

---

## 1. Build & Startup

### 1.1 Clean compilation
**Command:**
```bash
make
```
**Expected result:** Finishes with no errors or warnings, produces the `webserv` binary.
**How the server handles it:** Standard `Makefile` with `-Wall -Wextra -Werror` (or equivalent) across all translation units; no compiler-specific extensions since the project targets strict C++98.

### 1.2 Rebuild from scratch
**Command:**
```bash
make fclean && make
```
**Expected result:** Removes binary and objects, then rebuilds cleanly.
**How the server handles it:** `fclean` removes `*.o` and the executable; `make` re-triggers all rules from source, confirming there's no stale-object dependency issue.

### 1.3 Startup with default config
**Command:**
```bash
./webserv conf/default.conf
```
**Expected result:** Prints startup logs, binds successfully, does not crash or hang.
**How the server handles it:** `main.cpp` parses the config, `WebServer` opens one listening socket per `host:port`, registers each in the event loop, then blocks on `poll()` waiting for activity — not on I/O.

### 1.4 Startup with no config argument
**Command:**
```bash
./webserv
```
**Expected result:** Either falls back to a default config path, or exits with a clear usage message — never a silent crash.
**How the server handles it:** `main.cpp` checks `argc`; if no path is given it uses a hardcoded default path or prints `Usage: ./webserv [config file]` and exits with a non-zero status.

### 1.5 Startup with invalid config
**Command:**
```bash
echo "this is not valid config" > /tmp/bad.conf
./webserv /tmp/bad.conf
```
**Expected result:** Server prints a clear parsing error and exits — it does not start listening on a broken configuration.
**How the server handles it:** `ConfigParser` validates syntax and required directives (e.g. `listen`) before `WebServer` ever opens a socket; a `SyntaxError`/parse failure aborts startup deterministically.

### 1.6 Port already in use
**Command:**
```bash
./webserv conf/default.conf &
./webserv conf/default.conf   # second instance, same port
```
**Expected result:** Second instance fails to bind and exits with an error — does not hang or silently steal the port.
**How the server handles it:** `Listener::bind()` checks the return of `bind()`; `EADDRINUSE` is caught and reported, then the process exits cleanly (no zombie fd left open).

---

## 2. GET — Static Files

### 2.1 GET the index page
**Command:**
```bash
curl -i http://127.0.0.1:8001/
```
**Expected result:** `200 OK`, correct `Content-Type: text/html`, valid HTML body.
**How the server handles it:** `Router` matches `/` to the `/` location; `HttpRequestHandler` resolves the configured `index` file inside `root`, reads it, and `HttpResponseBuilder` sets headers and streams the body.

### 2.2 GET a specific existing file
**Command:**
```bash
curl -i http://127.0.0.1:8001/about.html
```
**Expected result:** `200 OK`, body matches the file on disk, `Content-Type` matches the extension (`text/html`).
**How the server handles it:** Path is resolved relative to the location's `root`; MIME type is looked up from a static extension→type table in `HttpResponseBuilder`.

### 2.3 GET a directory with autoindex on
**Command:**
```bash
curl -i http://127.0.0.1:8001/uploads/
```
**Expected result:** `200 OK` with an HTML listing of the directory's contents.
**How the server handles it:** When `autoindex on` is set for the location and no `index` file is present, the handler lists the directory (via `opendir`/`readdir`) and generates a simple HTML page on the fly.

### 2.4 GET a directory with autoindex off and no index file
**Command:**
```bash
curl -i http://127.0.0.1:8001/empty-dir/
```
**Expected result:** `403 Forbidden` (or `404`, depending on config) — never a raw file listing or crash.
**How the server handles it:** The handler checks `autoindex` before falling back to listing; if it's off and no index exists, it returns the configured error response instead.

### 2.5 GET a nonexistent file
**Command:**
```bash
curl -i http://127.0.0.1:8001/does-not-exist.html
```
**Expected result:** `404 Not Found`, custom error page if configured.
**How the server handles it:** `open()`/`stat()` on the resolved path fails; `RequestHandler` routes to `ErrorResponseFactory`, which serves the `error_page 404` target from config or a built-in default.

### 2.6 Path traversal attempt
**Command:**
```bash
curl -i "http://127.0.0.1:8001/../../../../etc/passwd"
```
**Expected result:** Request is blocked (`400` or `403`/`404`) — the file is never served.
**How the server handles it:** The resolved path is normalized/canonicalized and checked against the location's `root`; any resolution escaping `root` is rejected before `open()` is ever called.

### 2.7 GET with a disallowed method on that location
**Command:**
```bash
curl -i -X PUT http://127.0.0.1:8001/
```
**Expected result:** `405 Method Not Allowed`.
**How the server handles it:** `Router`/`RequestHandler` checks the resolved location's `methods` list before dispatching; a method not in that list short-circuits straight to a `405` response.

---

## 3. POST — Uploads & Body Handling

### 3.1 Upload a file
**Command:**
```bash
curl -i -X POST --data-binary @test.txt http://127.0.0.1:8001/upload
```
**Expected result:** `201 Created` (or equivalent success), file appears under the configured `upload_path`.
**How the server handles it:** `RequestHandler` detects `upload on` for the location, reads the body according to `Content-Length`, and writes it to `upload_path` via `FileUtils`.

### 3.2 Upload exceeding max body size
**Command:**
```bash
head -c 5000000 /dev/urandom > big.bin
curl -i -X POST --data-binary @big.bin http://127.0.0.1:8001/upload
```
**Expected result:** `413 Payload Too Large` — server does not crash or buffer unbounded data.
**How the server handles it:** `client_max_body_size` from config is checked against `Content-Length` (and against actual bytes received for chunked bodies) before/while buffering; exceeding it aborts the request early.

### 3.3 POST with no body
**Command:**
```bash
curl -i -X POST http://127.0.0.1:8001/upload
```
**Expected result:** Either a `400 Bad Request` or a defined "empty upload" behavior — not a hang.
**How the server handles it:** Parser treats a missing/zero `Content-Length` as "no body expected"; the handler either rejects or accepts an empty file depending on how upload handling is implemented.

### 3.4 POST without Content-Length or Transfer-Encoding
**Command:**
```bash
printf 'POST /upload HTTP/1.1\r\nHost: localhost\r\n\r\nsomebody' | nc 127.0.0.1 8001
```
**Expected result:** `400 Bad Request` (HTTP/1.1 requires one or the other for a body) — connection is closed cleanly.
**How the server handles it:** `HttpRequestParser` flags requests with a body but no length indicator as malformed rather than guessing where the body ends.

### 3.5 POST with chunked transfer-encoding *(if supported)*
**Command:**
```bash
printf 'POST /upload HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n4\r\ntest\r\n0\r\n\r\n' | nc 127.0.0.1 8001
```
**Expected result:** `201 Created`, file content equals `test`.
**How the server handles it:** *(If implemented)* the parser detects `Transfer-Encoding: chunked`, reads chunk-size lines and chunk data iteratively until the terminating `0\r\n\r\n`. *(If not implemented, note this explicitly as a known limitation rather than guessing.)*

---

## 4. DELETE

### 4.1 DELETE an existing file
**Command:**
```bash
curl -i -X DELETE http://127.0.0.1:8001/uploads/test.txt
```
**Expected result:** `204 No Content` (or `200`), file removed from disk.
**How the server handles it:** `RequestHandler` resolves the path under the location's `root`, calls `unlink()`/`remove()`, and returns success only if the filesystem call succeeds.

### 4.2 DELETE a file that doesn't exist
**Command:**
```bash
curl -i -X DELETE http://127.0.0.1:8001/uploads/test.txt
```
**Expected result:** `404 Not Found` — same command run twice in a row should fail the second time.
**How the server handles it:** `unlink()` failing with `ENOENT` is mapped to `404` rather than treated as success or a `500`.

### 4.3 DELETE on a location that forbids it
**Command:**
```bash
curl -i -X DELETE http://127.0.0.1:8001/
```
**Expected result:** `405 Method Not Allowed`.
**How the server handles it:** Same `methods` check as GET/POST — `/` location's config doesn't list `DELETE`, so it never reaches the filesystem logic.

### 4.4 DELETE a directory
**Command:**
```bash
curl -i -X DELETE http://127.0.0.1:8001/uploads/
```
**Expected result:** A defined error (`403`/`409`) — never a recursive delete, never a crash.
**How the server handles it:** The handler checks `stat()` type before calling `unlink()`; directories are explicitly rejected rather than passed to a call that would fail unpredictably (or, worse, recurse).

---

## 5. Errors, Redirects & Config-Driven Behavior

### 5.1 Custom 404 page
**Command:**
```bash
curl -i http://127.0.0.1:8001/nope
```
**Expected result:** `404 Not Found` with the **configured** custom error page body, not a generic default.
**How the server handles it:** `ErrorResponseFactory` looks up the status in the location/server's `error_page` map first, falling back to a built-in minimal page only if none is configured.

### 5.2 Redirect
**Command:**
```bash
curl -i http://127.0.0.1:8001/old-path
```
**Expected result:** `301 Moved Permanently` with `Location: /new-path` (or whatever target is configured).
**How the server handles it:** The location is marked as a redirect in config; `RequestHandler` short-circuits straight to building a redirect response without touching the filesystem.

### 5.3 Virtual host by Host header
**Command:**
```bash
curl -i -H "Host: site2.local" http://127.0.0.1:8001/
```
**Expected result:** Content from `site2`'s configured `root`, not `site1`'s — assuming two `server` blocks share the same `host:port` with different `server_name`s.
**How the server handles it:** `Router` matches the `Host` header against each server block's `server_name`; the correct config (root, index, error pages) is selected before the request is dispatched further.

### 5.4 Unknown Host header on a multi-server port
**Command:**
```bash
curl -i -H "Host: unknown.local" http://127.0.0.1:8001/
```
**Expected result:** Falls back to the default/first server block for that `host:port`, per config semantics — documented, not accidental.
**How the server handles it:** `Router` defines an explicit fallback (first matching `listen` block) when no `server_name` matches, matching common NGINX-like behavior.

---

## 6. CGI

### 6.1 Run a working CGI script
**Command:**
```bash
curl -i http://127.0.0.1:8001/cgi-bin/hello.py
```
**Expected result:** `200 OK`, dynamic body generated by the script (not a static file).
**How the server handles it:** `RequestHandler` detects the `.py` extension mapped to a CGI interpreter in config; `CgiJob` forks, execs the interpreter with the script path, sets CGI environment variables, and `CgiOutputParser` turns the script's stdout into a proper HTTP response.

### 6.2 CGI with query string
**Command:**
```bash
curl -i "http://127.0.0.1:8001/cgi-bin/hello.py?name=Diana"
```
**Expected result:** `200 OK`, script output reflects the query parameter.
**How the server handles it:** The query string is extracted during parsing and passed to the CGI process via the `QUERY_STRING` environment variable, per RFC 3875.

### 6.3 CGI with POST body
**Command:**
```bash
curl -i -X POST --data "field=value" http://127.0.0.1:8001/cgi-bin/echo.py
```
**Expected result:** `200 OK`, script receives and can process the POST body.
**How the server handles it:** The request body is piped to the CGI process's stdin, with `CONTENT_LENGTH` and `REQUEST_METHOD` set accordingly.

### 6.4 CGI script that fails/crashes
**Command:**
```bash
curl -i http://127.0.0.1:8001/cgi-bin/broken.py
```
**Expected result:** `500 Internal Server Error` — not a hang, not a server crash.
**How the server handles it:** A non-zero exit code or malformed CGI output (missing headers) from the child process is caught by `CgiOutputParser`/`CgiJob` and translated into a `500` rather than propagated raw.

### 6.5 CGI script that hangs (infinite loop)
**Command:**
```bash
curl -i http://127.0.0.1:8001/cgi-bin/infinite_loop.py
```
**Expected result:** Either the request times out and returns an error after a bounded time, *or* — if no timeout is implemented — this is flagged as a known limitation. What must **not** happen: the whole server freezing for all other clients.
**How the server handles it:** *(State honestly whichever is true.)* If implemented: a timer/deadline tracked per CGI job kills the child (`kill()`/`SIGKILL`) after N seconds and the connection is closed with an error. If not implemented: say so directly in the defense — this is one of the most common evaluator probes.

### 6.6 CGI on a disallowed method
**Command:**
```bash
curl -i -X DELETE http://127.0.0.1:8001/cgi-bin/hello.py
```
**Expected result:** `405 Method Not Allowed`.
**How the server handles it:** Same location `methods` check applies before CGI dispatch — CGI is not a bypass around method restrictions.

---

## 7. HTTP Protocol Edge Cases

### 7.1 Malformed request line
**Command:**
```bash
printf 'NOTAMETHOD /\r\n\r\n' | nc 127.0.0.1 8001
```
**Expected result:** `400 Bad Request`, connection closed cleanly.
**How the server handles it:** `HttpRequestParser` validates the request line has exactly method/path/version in the expected format before attempting to route anything.

### 7.2 Missing Host header (HTTP/1.1)
**Command:**
```bash
printf 'GET / HTTP/1.1\r\n\r\n' | nc 127.0.0.1 8001
```
**Expected result:** `400 Bad Request` — `Host` is mandatory in HTTP/1.1.
**How the server handles it:** Parser/`Router` requires a `Host` header for HTTP/1.1 requests before virtual-host resolution can proceed at all.

### 7.3 Multiple pipelined requests on one connection
**Command:**
```bash
printf 'GET / HTTP/1.1\r\nHost: localhost\r\n\r\nGET /about.html HTTP/1.1\r\nHost: localhost\r\n\r\n' | nc 127.0.0.1 8001
```
**Expected result:** Both responses are returned in order over the same connection.
**How the server handles it:** `ClientConnection` keeps parsing from its read buffer after completing one request/response cycle, rather than discarding leftover bytes — each full request in the buffer is dispatched in turn.

### 7.4 Slow / partial request (byte-by-byte)
**Command:**
```bash
(printf 'GET '; sleep 1; printf '/ HTTP/1.1\r\n'; sleep 1; printf 'Host: localhost\r\n\r\n') | nc 127.0.0.1 8001
```
**Expected result:** Server waits and eventually responds correctly — no premature parse error, no blocking of other clients in the meantime.
**How the server handles it:** The event loop only calls `read()` when `poll()` reports the socket as readable; partial data accumulates in a per-connection buffer until a full request is available, all while other connections continue to be served.

### 7.5 Client disconnects mid-request
**Command:**
```bash
(printf 'GET / HTTP/1.1\r\n'; sleep 0.2) | timeout 0.1 nc 127.0.0.1 8001
```
**Expected result:** Server detects the disconnect and cleans up — no leaked file descriptor, no crash.
**How the server handles it:** A `read()` returning `0` (or a poll hangup event) triggers `ClientConnection` cleanup: the fd is closed and removed from the poll set.

### 7.6 Very long header line
**Command:**
```bash
python3 -c "print('GET / HTTP/1.1\r\nHost: localhost\r\nX-Long: ' + 'a'*100000 + '\r\n\r\n', end='')" | nc 127.0.0.1 8001
```
**Expected result:** A defined rejection (`400`/`431`), not unbounded memory growth.
**How the server handles it:** The parser enforces a maximum header size while accumulating the header buffer, rejecting the request once the limit is exceeded instead of growing the buffer indefinitely.

---

## 8. Concurrency & Resilience

### 8.1 Many simultaneous clients
**Command:**
```bash
for i in $(seq 1 50); do curl -s -o /dev/null -w "%{http_code}\n" http://127.0.0.1:8001/ & done; wait
```
**Expected result:** All 50 requests return `200`, server stays responsive throughout.
**How the server handles it:** The single `poll()` loop multiplexes all client sockets on one thread; no request blocks another since each read/write only happens when the socket is actually ready.

### 8.2 File descriptor leak check
**Command:**
```bash
lsof -p $(pgrep webserv) | wc -l
# run a batch of requests, then check again
for i in $(seq 1 100); do curl -s -o /dev/null http://127.0.0.1:8001/; done
lsof -p $(pgrep webserv) | wc -l
```
**Expected result:** FD count returns to roughly its baseline after the requests complete — not growing unbounded.
**How the server handles it:** Every accepted connection is guaranteed a matching `close()` when it's done or disconnected, via `ClientConnection`'s destructor/cleanup path in the event loop.

### 8.3 Stress test with a load tool
**Command:**
```bash
ab -n 1000 -c 20 http://127.0.0.1:8001/
# or: siege -c 20 -r 50 http://127.0.0.1:8001/
```
**Expected result:** Completes with a low/zero failure rate and no crash.
**How the server handles it:** Same non-blocking event-loop design as above — throughput is bounded by CPU/IO, not by the concurrency model breaking down.

### 8.4 Kill client mid-upload, then reuse the server
**Command:**
```bash
head -c 50000000 /dev/urandom > huge.bin
curl -X POST --data-binary @huge.bin http://127.0.0.1:8001/upload & 
sleep 0.1; kill %1
curl -i http://127.0.0.1:8001/     # server should still respond normally
```
**Expected result:** The killed upload doesn't corrupt server state; a normal request right after still gets a clean `200`.
**How the server handles it:** Each connection's state is isolated in its own `ClientConnection` object; aborting one doesn't touch the event loop's ability to serve others.

---

## 9. Full Automated Suite

**Command:**
```bash
./tests/run_tests.sh
```
**Expected result:** All tests pass; final line shows a clear success summary (e.g. `ALL TESTS PASSED (42/42)`).
**How the server handles it:** This script exercises the cases above end-to-end against a running instance and diffs actual vs. expected status codes/bodies, serving as the single command to point to when asked "how do we know this works."

---

## Summary Table (for a quick glance during defense)

| # | Test | Expect |
|---|---|---|
| 1.x | Build/startup | Clean compile, no crash, rejects bad config/ports |
| 2.x | GET | 200/403/404/405, traversal blocked |
| 3.x | POST/upload | 201, 413 on oversize, 400 on ambiguous body |
| 4.x | DELETE | 204/404/405, directories rejected |
| 5.x | Errors/redirects/vhosts | Custom pages, 301, correct Host routing |
| 6.x | CGI | Dynamic output, 500 on crash, hang behavior known |
| 7.x | Protocol edge cases | 400 on malformed/missing Host, pipelining works |
| 8.x | Concurrency | No blocking, no FD leaks, survives stress + aborts |
| 9 | Automated suite | All green |