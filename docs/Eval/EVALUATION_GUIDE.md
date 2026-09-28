# Webserv: project explanation and mandatory evaluation guide

## Start with this project explanation

> “Webserv is our HTTP/1.1 server written in C++98. Its purpose is to understand what happens between a browser sending a request and a server returning a response. We implemented TCP socket management, HTTP parsing, routing, static file handling, uploads, deletion, and CGI execution ourselves. A configuration file defines listening addresses, virtual hosts and routes.
>
> We divided the implementation into modules: configuration, networking, HTTP, routing, CGI and utilities. At startup, we parse the configuration and create one listener per unique address and port. A single `poll()` loop monitors listeners, clients and the parent-side CGI pipes. Each client has a state and buffers, so incomplete requests and partial writes can continue over several loop iterations. The router selects a virtual host using `Host`, then the longest matching location. We either produce a response immediately or start a CGI child and collect its output. The response builder creates the HTTP message, and the connection closes after transmission.”

The original failures below are historical; consult the current verification and fix notes for their resolution. There is no working persistent-connection lifecycle here: `ClientConnection::writeToSocket()` marks the client done after its response.

**Useful opening demonstration — repository root:**

```bash
cd /home/diana/Documents/webserv
cat src/main.cpp
cat src/core/WebServer.cpp
rg -n 'poll\(|POLLIN|POLLOUT' src/network/EventLoop.cpp
```

Explain the chain: `main → ConfigParser → WebServer::setup → EventLoop::run → ClientConnection → RequestHandler → Router / HttpRequestHandler / CgiJob → HttpResponseBuilder`.

## Original audit result — historical, before fixes

**The project is not ready to pass this exact checklist.** The 39 existing tests pass, but the CGI `errno` branches directly violate its automatic-zero rule. The added behavioral audit also exposes configuration and CGI failures that the existing tests do not cover.

| Check | Observed result |
|---|---|
| Clean build; second `make` | PASS; second invocation does no work |
| Existing suite | 39/39 pass; this includes two compiled unit-test executables |
| Added behavioral audit | 19/29 pass; ten failure probes reproduced |
| Single main `poll()` | Present; monitors sockets and CGI pipes |
| No `errno` control flow after I/O | **FAIL**, `src/cgi/CgiJob.cpp:154,180` |
| Configured `index home.html` | **FAIL**, serves hardcoded `index.html` |
| Configured custom 404 path | **FAIL** for static missing-file path; falls back to root/errors or generated error |
| Autoindex | **FAIL** at location root without index; nested listing works even with `autoindex off` |
| Upload destination and disabling | **FAIL**, uses root/uploads and accepts POST with `upload off` |
| Body size | PASS at 64-byte limit; 65 bytes returns 413 |
| Route roots, hosts, ports, address separation | PASS in tested cases |
| GET / POST / DELETE | Basic operation works; **204 response framing is invalid** |
| Unknown method | Does not crash; returns 405 rather than the appropriate 501 for an unimplemented method |
| CGI GET / POST / relative file / crashing script | PASS; crashing script returns 500 |
| Infinite CGI | **FAIL**, closes client around 30 seconds without HTTP error |
| CGI closes stdout then sleeps | **FAIL**, blocks other requests in `waitpid(..., 0)` |
| Short Siege sample | 3 × 2,000 requests, 20 clients, 100% availability, zero failures |
| Static-load memory / descriptors | RSS 4,188 → 4,196 → 4,196 → 4,196 KiB; descriptors return to 6 |
| Valgrind sample | 0 errors; 0 definitely/indirectly/possibly lost bytes; 109,143 bytes still reachable at SIGTERM |
| Timed Siege endurance attempt | INCONCLUSIVE: Siege did not exit at requested 30 seconds, produced no final stats, required SIGKILL; separate GET still returned 200 in 0.000601 seconds |
| Browser GUI / indefinite endurance | Manual follow-up; not established by the automated run |

Evidence: [existing suite](results/existing-suite.log), [additional audit](results/audit.json), [build](results/build.log), [Siege](results/siege-0.log), [memory samples](results/stress-metrics.json), [Valgrind](results/valgrind.log). Valgrind covered 100 static requests, 20 CGI POSTs and a large-file transfer. Termination used SIGTERM, so the program's normal destructors did not clean up live server allocations/listeners. “Still reachable” is not proof of a leak, and this sample does not prove every error path is leak-free.

## One-time demo setup: exactly where to run commands

Use Bash on this Linux machine. **Every shell command in this guide runs from `/home/diana/Documents/webserv` unless explicitly marked as browser input.** Relative repository paths depend on that directory. Run the numbered examples in order. The fixtures live in a new temporary directory, so upload/delete/error-page demonstrations do not overwrite the real website.

In **Terminal A**:

```bash
cd /home/diana/Documents/webserv
make
export EVAL_DIR="$(python3 docs/Eval/evaluation_audit.py --setup)"
printf '%s\n' "$EVAL_DIR"
./webserv "$EVAL_DIR/audit.conf" > "$EVAL_DIR/demo-server.log" 2>&1 &
export SERVER_PID=$!
printf 'Server PID: %s\n' "$SERVER_PID"
```

Expected: build succeeds; fixture path is `/tmp/webserv-evaluation-...`; server stays running. Inspect `cat "$EVAL_DIR/demo-server.log"` for listeners on `127.0.0.1:18880`, `127.0.0.1:18881` and `127.0.0.2:18880`. If a port is occupied, stop your previous demo instance; do not test a different process accidentally.

Continue commands in Terminal A. For concurrent commands, open **Terminal B**, `cd` to the repository root, and copy the values printed above:

```bash
export EVAL_DIR=/tmp/webserv-evaluation-REPLACE_WITH_PRINTED_VALUE
export SERVER_PID=REPLACE_WITH_PRINTED_PID
```

These are the only placeholders to copy from your own run. The fixture config is fully written by `--setup`; inspect it with:

```bash
cat "$EVAL_DIR/audit.conf"
```

`curl -i` includes response headers; `-v` also shows outgoing headers; `-X` selects a method; `--data-binary` preserves input bytes; `--max-time` bounds waiting; `--noproxy '*'` makes local address tests bypass proxy settings. Do not use `curl -I` for GET demonstrations: it sends HEAD, which this implementation rejects.

To repeat automated checks, first stop the demo (`kill "$SERVER_PID"; wait "$SERVER_PID"`), then:

```bash
python3 tests/runner.py
python3 docs/Eval/evaluation_audit.py
```

The second command creates its own fixtures, launches its own process group, stops it after testing, prints results and an artifact directory, and returns exit 1 while findings remain. It takes about 40 seconds plus load time. Its known failing checks are intentional regression tests, not a failure to run the audit.

## A. README compliance — all questions

### 1. Is README.md at the root, with the exact italicized first line?

```bash
test -f README.md && head -n 1 README.md
```

Expected literal format for this team:

```markdown
*This project has been created as part of the 42 curriculum by mjusta, lformnak, dkolarov.*
```

**Observed:** README exists and is italicized, but logins additionally use `**bold**`. Its rendered sentence is correct, but the source is not the exact format in the checklist. Remove the inner bold markers to eliminate this strict-format risk. This audit did not edit the README.

### 2. Does it contain a Description explaining purpose and overview?

```bash
sed -n '/^## Description/,/^## Instructions/p' README.md
```

**Expected/observed:** description of the C++98 HTTP server and its modules is present. Some claims overstate current compliance; the defects in this guide take precedence over those claims.

### 3. Does Instructions explain compilation and execution?

```bash
sed -n '/^## Instructions/,/^## Project Structure/p' README.md
```

**Observed:** yes, but the documented port 8080 differs from `conf/default.conf`'s **8001**. `./tests/run_tests.sh` and `docs/TESTING.md` are absent; the working runner is `python3 tests/runner.py`. Do not repeat the stale README commands during the defense.

### 4. Does Resources include references and an explicit AI-use explanation?

```bash
sed -n '/^## Resources/,$p' README.md
```

**Expected/observed:** references and AI-use tasks are listed. Team members must confirm the accuracy of their authorship declaration; runtime testing cannot verify historical authorship. The evaluator says missing required elements means zero.

## B. Code review and theory — all questions

### 5. How do we install Siege?

The checklist's normal installation command, from the repository root or any directory:

```bash
sudo apt-get install siege
siege --version
```

Expected: installation completes and a Siege version prints. During this audit, Siege was downloaded and unpacked under `/tmp/webserv-siege-tool` without changing system packages. The stress section gives both ways to invoke it.

### 6. What are the basics of an HTTP server?

> “A server listens on a TCP address and port, accepts connections, parses requests and returns responses. A request has a method, target, HTTP version, headers and possibly a body. A response has a status line, headers and possibly a body. HTTP describes the message; TCP carries its bytes. TCP can split or combine data, so one `recv()` is not necessarily one request.”

```bash
curl --noproxy '*' -v http://127.0.0.1:18880/index.html
```

Expected: outgoing `GET /index.html HTTP/1.1`, `Host`, incoming `200 OK`, `Content-Length: 14`, `Connection: close`, body `PRIMARY INDEX`. Explain that Content-Length counts body bytes, not headers.

### 7. Which event mechanism do we use?

> “We use POSIX `poll()` in a single-threaded event loop. Each monitored descriptor has an `events` mask describing what we want and a `revents` mask describing what the kernel reports.”

```bash
rg -n '\bpoll\s*\(' src
```

Expected: one real call in `EventLoop::run`; other matches may be logging strings. Socket readiness and CGI pipe readiness share this poll loop.

### 8. How does poll work?

> “`POLLIN` means we can attempt a read or accept. `POLLOUT` means we can attempt a write. Readiness does not guarantee a whole HTTP message or an entire output buffer will fit. `poll()` returns the number of ready descriptors, zero on timeout, or -1 on failure. We inspect `revents`, do the permitted work, update client state and return to the loop.”

```bash
sed -n '269,316p' src/network/EventLoop.cpp
sed -n '115,138p' src/network/EventLoop.cpp
```

Expected: `poll(..., 1000)` and state-dependent `POLLIN`/`POLLOUT` masks. Listening sockets stay readable-only. The one poll call monitors read interests and write interests together across its descriptor set; idle clients do not need constant `POLLOUT` subscriptions.

### 9. Is there only one poll, and how are accept/read/write handled?

> “Listeners get `POLLIN`. New accepted sockets become nonblocking client objects. Reading clients subscribe to `POLLIN`; clients with a response subscribe to `POLLOUT`. After each event we update or remove them. CGI input pipes subscribe to `POLLOUT`, and output pipes to `POLLIN`.”

```bash
cat src/network/Listener.cpp
sed -n '140,270p' src/network/EventLoop.cpp
```

Expected: `acceptClient()` creates clients; `fcntl(..., O_NONBLOCK)` prevents socket calls from waiting indefinitely. **Review issue:** the listener dispatch calls `_handleListenerEvent(fd)` for any nonzero `revents`; it should explicitly distinguish `POLLIN` from error-only events before accepting.

### 10. Show the path from poll readiness to client I/O; is all event I/O gated?

```bash
rg -n 'revents|readFromSocket|writeToSocket|readFromCgi|writeToCgi' src/network/EventLoop.cpp
rg -n '\b(recv|send|read|write)\s*\(' src/network/ClientConnection.cpp src/cgi/CgiJob.cpp
```

Explain: `run → _handleClientEvent → POLLIN → readFromSocket → recv`; similarly `POLLOUT → writeToSocket → send`. CGI parent I/O uses `_handleJobEvent` and readiness masks. No hidden secondary polling loop was found. Child CGI programs use their conventional stdin/stdout interface; distinguish them from parent server pipe management.

**Updated logging behavior:** Logger checks the destination with `fstat` and writes only to regular files. Pipe, socket and TTY diagnostics are suppressed. Run `./webserv conf/default.conf > /tmp/webserv.log 2>&1` to retain diagnostics without unpolled event-descriptor writes.

### 11. On socket read/write errors, is the client removed?

```bash
sed -n '60,123p' src/network/ClientConnection.cpp
sed -n '158,194p' src/network/EventLoop.cpp
```

> “`recv` or `send` returning zero or a negative value marks the client done. The event loop sees `isDone()`, removes the poll entry and deletes the client; its destructor closes the socket.”

**Observed:** this socket path matches the supplied no-errno policy. Do not claim that all CGI error/cleanup paths are equally correct.

### 12. Are negative, zero and positive return values all handled?

```bash
rg -n 'bytes <= 0|bytes >|n > 0|n == -1|loaded >= 0|loaded == 0|loaded == -1' src/network/ClientConnection.cpp src/cgi/CgiJob.cpp
```

> “Positive values are the actual byte count. We append exactly received bytes or advance the output offset by exactly written bytes. Zero from a read means EOF; negative means an error. Partial output remains buffered for a later writable event.”

**Observed:** sockets handle both nonpositive cases and retain unsent bytes. CGI code distinguishes EOF/errors too, but uses forbidden errno branches and a blocking wait during cleanup.

### 13. Do we check errno after read/recv/write/send?

```bash
rg -n 'errno|EAGAIN|EWOULDBLOCK' src
```

**FAIL:** `CgiJob.cpp:154` and `:180` use errno to choose control flow after pipe `write` and `read`. Configuration parsing's `errno == ERANGE` after numeric conversion is unrelated. In ordinary POSIX code, EAGAIN handling is common; this supplied evaluation explicitly forbids these post-I/O errno branches. Its wording also mentions EAGAIN elsewhere, but the explicit automatic-zero rule cannot be ignored.

### 14. Are regular-file operations acceptable, and can they stall the loop?

> “The checklist exempts ordinary disk files from readiness polling. Config is read before serving clients. Static files and uploads are synchronous, and complete files/bodies are held in memory. Small local files performed well here, but this architecture cannot promise that slow storage or very large files never delay the loop.”

```bash
cat src/utils/FileUtils.cpp
rg -n 'ofstream|out.write|FileUtils::read' src
python3 -c 'import os; f=open(os.environ["EVAL_DIR"]+"/site/large.bin","wb"); f.truncate(16*1024*1024); f.close()'
curl --noproxy '*' --limit-rate 64k --max-time 10 -o /dev/null http://127.0.0.1:18880/large.bin &
curl --noproxy '*' -s -o /dev/null -w 'status=%{http_code} seconds=%{time_total}\n' http://127.0.0.1:18880/empty.html
```

Expected: small request remains responsive while a slow reader receives the large file. First curl is intentionally time-limited and can exit 28. In the recorded Valgrind sample, the concurrent small request returned 200 in about 0.18 seconds. This is a finite responsiveness test, not a disk-latency guarantee.

### 15. Does it compile cleanly without relinking?

Stop your demo process before replacing the executable, then:

```bash
make re
make
```

Expected/observed: first builds with `-Wall -Wextra -Werror -std=c++98`; second says `Nothing to be done for 'all'.` Restart with the setup launch command before continuing. Under the checklist, an invalid compile or unclear mandatory code-review point stops evaluation; this audit continued to reveal the remaining defects.

## C. Configuration — all questions

### 16. Which HTTP status codes are appropriate?

Use the official [IANA status registry](https://www.iana.org/assignments/http-status-codes/http-status-codes.xhtml) and [HTTP semantics, RFC 9110](https://www.rfc-editor.org/rfc/rfc9110.html).

| Code | Meaning in these demonstrations |
|---|---|
| 200 | GET or CGI completed successfully |
| 201 | Upload created a resource |
| 204 | DELETE succeeded, no response body |
| 301 / 302 | Redirect; inspect `Location` |
| 400 | Malformed request |
| 403 | Operation/resource forbidden |
| 404 | Resource missing |
| 405 | Implemented method prohibited for this resource; must include `Allow` |
| 413 | Request body exceeds configured limit |
| 500 | Internal failure, such as the tested crashing CGI |
| 501 | Method not implemented by the server |
| 504 | Suitable timeout response when waiting for CGI |

A 204 response must not include content or Content-Length. Your server sends both. Its 405 responses also omit `Allow`. See [RFC 9110, Content-Length](https://www.rfc-editor.org/rfc/rfc9110.html#section-8.6), [405](https://www.rfc-editor.org/rfc/rfc9110.html#section-15.5.6), and [501](https://www.rfc-editor.org/rfc/rfc9110.html#section-15.6.2). “Payload Too Large” is an older phrase for 413; the numeric meaning is the important issue here.

### 17. Can multiple websites use different addresses and ports?

```bash
curl --noproxy '*' -i http://127.0.0.1:18880/index.html
curl --noproxy '*' -i http://127.0.0.1:18881/
curl --noproxy '*' -i http://127.0.0.2:18880/
```

Expected/observed: 200 with `PRIMARY INDEX`, then twice 200 with `SECOND SITE`. The fixture has separate `listen` directives. The second loopback address demonstrates address-specific binding on Linux; it is not a test of a second physical network card.

### 18. Can we configure and modify the default 404 page?

```bash
printf 'CHANGED CUSTOM ERROR\n' > "$EVAL_DIR/custom404.html"
curl --noproxy '*' -i http://127.0.0.1:18880/missing
```

Expected: 404 with `CHANGED CUSTOM ERROR` because `error_page 404` points to this file. **Actual: FAIL**, generated 404 body. Static handler errors call the root-only error factory and lose the configured mapping. Existing tests pass because their files happen to be under the conventional `root/errors/404.html` path. A configured mapping must work outside that convention too.

### 19. Can we limit the client request body?

```bash
printf 'short body' > "$EVAL_DIR/small.txt"
python3 -c 'import os; open(os.environ["EVAL_DIR"]+"/large.txt","w").write("x"*65)'
curl --noproxy '*' -i -H 'Content-Type: text/plain' --data-binary @"$EVAL_DIR/small.txt" http://127.0.0.1:18880/upload/small.txt
curl --noproxy '*' -i -H 'Content-Type: text/plain' --data-binary @"$EVAL_DIR/large.txt" http://127.0.0.1:18880/upload/large.txt
```

Expected/observed: 201, then 413. The fixture limit is 64 bytes. `--data-binary` implicitly selects POST. `text/plain` is the conventional media type; the supplied checklist's `plain/text` spelling is unnecessary. The added audit also tested exactly 64 bytes (accepted).

### 20. Can routes point to different directories?

```bash
curl --noproxy '*' -i http://127.0.0.1:18880/index.html
curl --noproxy '*' -i http://127.0.0.1:18880/other/
```

Expected/observed: `PRIMARY INDEX`, then `SECOND SITE`, both 200. `/other` maps to the fixture's `other` directory. Existing tests also exercise longest location matching and segment boundaries.

### 21. Can the default directory file be changed?

```bash
rg 'index' "$EVAL_DIR/audit.conf"
cat "$EVAL_DIR/site/home.html"
curl --noproxy '*' -i http://127.0.0.1:18880/
```

Expected: configured `home.html`, body `CONFIGURED INDEX`. **Actual: FAIL**, returns `PRIMARY INDEX` from `index.html`. `HttpRequestHandler` hardcodes the index filename instead of receiving the configured value.

### 22. Can accepted methods vary by route?

```bash
curl --noproxy '*' -i -X DELETE http://127.0.0.1:18880/index.html
printf 'temporary file\n' > "$EVAL_DIR/site/uploads/allowed.txt"
curl --noproxy '*' -i -X DELETE http://127.0.0.1:18880/uploads/allowed.txt
```

Expected/observed status: 405 for root GET-only route; 204 on `/uploads`, which permits GET and DELETE. Expected complete protocol behavior additionally requires `Allow` on 405 and no body/Content-Length on 204; these are currently missing/incorrect.

## D. Basic checks — all questions

### 23. Can GET be demonstrated with telnet as well as curl?

```bash
telnet 127.0.0.1 18880
```

Then type these lines and press Enter twice after the Host line:

```http
GET /index.html HTTP/1.1
Host: primary.test

```

Expected: 200 response and `PRIMARY INDEX`, then the connection closes. This is a manual telnet demonstration, not a command to paste directly into Bash. Curl equivalent:

```bash
curl --noproxy '*' -i http://127.0.0.1:18880/index.html
```

### 24. Can POST upload a prepared file and GET retrieve its exact contents?

```bash
printf 'hello from evaluation\n' > "$EVAL_DIR/input.txt"
curl --noproxy '*' -i --data-binary @"$EVAL_DIR/input.txt" http://127.0.0.1:18880/upload/demo.txt
curl --noproxy '*' -sS http://127.0.0.1:18880/uploads/demo.txt -o "$EVAL_DIR/download.txt"
cmp "$EVAL_DIR/input.txt" "$EVAL_DIR/download.txt" && printf 'Exact bytes match\n'
ls -l "$EVAL_DIR/custom-uploads" "$EVAL_DIR/site/uploads"
```

Expected basic round trip: 201, then identical bytes and `Exact bytes match`. **Observed:** round trip works using actual `site/uploads`. **Configuration failure:** upload was supposed to go to `custom-uploads`. Do not present that as correct `upload_path` behavior. Use `--data-binary`; multipart `curl -F` parsing is not established by these tests.

Also isolate the upload-enable flag from method restrictions:

```bash
curl --noproxy '*' -i --data-binary 'must not upload' http://127.0.0.1:18880/disabled/off.txt
```

Expected: a rejection, normally 403 or 405. **Actual: 201**, despite `upload off`; POST is allowed on this route specifically to expose the flag being ignored.

### 25. Can DELETE remove an uploaded file?

After question 24:

```bash
curl --noproxy '*' -i -X DELETE http://127.0.0.1:18880/uploads/demo.txt
curl --noproxy '*' -i http://127.0.0.1:18880/uploads/demo.txt
test ! -e "$EVAL_DIR/site/uploads/demo.txt" && printf 'File removed\n'
```

Expected/observed: 204, then 404 and `File removed`. **Framing failure:** normal curl and `http.client` hide the illegal body after 204. Show the wire bytes separately:

```bash
printf 'delete me\n' > "$EVAL_DIR/site/uploads/raw.txt"
python3 - <<'PY'
import socket
with socket.create_connection(('127.0.0.1',18880),timeout=3) as s:
    s.sendall(b'DELETE /uploads/raw.txt HTTP/1.1\r\nHost: primary.test\r\n\r\n')
    data=b''
    while True:
        part=s.recv(4096)
        if not part: break
        data+=part
    print(repr(data))
PY
```

Expected: headers ending at `\r\n\r\n`, without Content-Length or body. Actual: `Content-Length: 49` and an HTML “204 No Content” body. This must be corrected before claiming HTTP compliance.

### 26. Do UNKNOWN requests avoid crashing the server?

```bash
curl --noproxy '*' -i -X UNKNOWN http://127.0.0.1:18880/
curl --noproxy '*' -i http://127.0.0.1:18880/index.html
```

Expected: 501 for an unimplemented method, then 200 proving continued service. **Actual:** 405, then 200. Crash-resistance passes; status semantics need correction. Unsupported methods and supported-but-disallowed methods should be distinguished.

### 27. Does every test return the correct status?

Use the table in question 16, and run:

```bash
python3 docs/Eval/evaluation_audit.py
```

Run this only after stopping the demo, since the audit owns the same ports. **Current answer: no**—see unknown method, 204 framing, autoindex/configuration behavior and infinite CGI findings. Several existing tests check only a status or accept an early connection close, so 39/39 is not equivalent to this checklist passing.

## E. CGI — all questions

### 28. What is CGI, and does it work with GET?

> “CGI lets the server execute a program to generate dynamic output. The parent sends the request body through a pipe to the child's stdin, supplies request metadata through environment variables, and reads CGI headers/body from stdout. We use `fork`, `dup2`, `chdir` and `execve`; the interpreter runs in the child, while the parent monitors its pipes.”

```bash
curl --noproxy '*' -i 'http://127.0.0.1:18880/cgi-bin/echo.py?name=Diana'
```

Expected/observed: 200, `Content-Type: text/plain`, `method=GET`, `query=name=Diana`.

### 29. Does CGI work with POST?

```bash
curl --noproxy '*' -i -H 'Content-Type: text/plain' --data-binary 'hello=world' http://127.0.0.1:18880/cgi-bin/echo.py
```

Expected/observed: 200, `method=POST`, `type=text/plain`, `hello=world`. This validates passing the body into CGI stdin, not merely serving a script file as text.

### 30. Is CGI executed in the correct directory for relative files?

```bash
cat "$EVAL_DIR/cgi/relative.py"
curl --noproxy '*' -i http://127.0.0.1:18880/cgi-bin/relative.py
```

Expected/observed: 200 and `RELATIVE OK`. Script uses `open("relative.txt")`; the file exists next to the script, not in the repository root. `CgiJob::start` changes into the script directory. Review note: return values from `chdir` and `dup2` are not checked.

### 31. What happens when CGI contains an error?

```bash
cat "$EVAL_DIR/cgi/error.py"
curl --noproxy '*' -i --max-time 5 http://127.0.0.1:18880/cgi-bin/error.py
curl --noproxy '*' -i http://127.0.0.1:18880/index.html
```

Expected/observed: 500 `CGI failed`, then 200. The Python exception appears in the server log. A child script failure must not terminate the main server.

### 32. What happens with an infinite CGI? Does it return an error and remain responsive?

```bash
curl --noproxy '*' -i --max-time 35 http://127.0.0.1:18880/cgi-bin/loop.py
curl --noproxy '*' -i --max-time 2 http://127.0.0.1:18880/index.html
```

Expected: bounded 5xx timeout response (typically 504), then 200. **Actual:** around 30 seconds the client is closed without any HTTP response; curl normally reports `Empty reply from server`. The ordinary page still responds. `_removeTimedOutCgi()` exists but is never called; ordinary client timeout currently removes the waiting client instead of preparing an HTTP error.

### 33. Does a CGI failure ever hang the whole server?

In Terminal A:

```bash
curl --noproxy '*' -i --max-time 12 http://127.0.0.1:18880/cgi-bin/close_sleep.py &
sleep 1
curl --noproxy '*' -i --max-time 2 http://127.0.0.1:18880/index.html
wait $!
```

This script closes its output descriptors and then sleeps eight seconds. Expected: the unrelated static request answers promptly. **Actual: FAIL**, the second request times out while the event loop blocks in `CgiJob::closeStdout → waitpid(..., 0)`. Service resumes when the child exits. Parent-side child reaping needs to avoid blocking the event loop.

Do not keep this deliberately disruptive test running during the browser or Siege demonstration.

## F. Browser — all questions

### 34. Open the chosen browser and inspect request/response headers

Use Firefox. Start the original demo site separately in Terminal B:

```bash
cd /home/diana/Documents/webserv
./webserv conf/default.conf > "$EVAL_DIR/browser-server.log" 2>&1 &
export BROWSER_SERVER_PID=$!
firefox http://127.0.0.1:8001/
```

**Browser action:** press F12, open Network, reload, select the document. Inspect request method/path/Host and response status, Content-Type, Content-Length and Connection. Expected: GET `/`, status 200 and rendered site1 page. This GUI inspection is a manual follow-up; the audit verified HTTP/static assets but did not inspect the live Network panel.

### 35. Can it serve a complete static website?

In Firefox Network, reload the page with cache disabled; inspect CSS, JS and image responses. Expected: assets are 200, MIME types appropriate, styles/images visible and browser console free of load errors. The existing suite passed CSS/JS/image GET tests; that does not establish visual correctness.

### 36. What happens for an incorrect URL?

**Browser address bar:** `http://127.0.0.1:8001/definitely-not-here`.

Expected: a visible error page and **404** in Network, not a successful blank page. Default site's conventional 404 file works; question 18 shows the separate failure when a configured error file lives elsewhere.

### 37. Can we list a directory?

**Browser address bars:**

- `http://127.0.0.1:18880/listing/` — should list `a.txt` and `nested`; currently 404.
- `http://127.0.0.1:18880/listing/nested/` — currently lists `a.txt` with 200.
- `http://127.0.0.1:18880/private/nested/` — should not list files with `autoindex off`; currently exposes `secret.txt` with 200.

The distinction demonstrates both the location-root bug and the ignored autoindex flag.

### 38. Does a redirected URL work?

**Browser address bar:** `http://127.0.0.1:18880/old`.

Expected: Network first shows 301 and `Location: /`, then the browser requests `/` and displays the page. Use “Persist Logs” to retain both requests. Terminal equivalent:

```bash
curl --noproxy '*' -i http://127.0.0.1:18880/old
curl --noproxy '*' -i -L http://127.0.0.1:18880/old
```

First shows the redirect; `-L` follows it. The audit verified the first response and Location header.

### 39. Try additional malformed/slow requests

```bash
python3 - <<'PY'
import socket
with socket.create_connection(('127.0.0.1',18880),timeout=3) as s:
    s.sendall(b'GET / HTTP/1.1\r\nHost: bad host\r\n\r\n')
    print(s.recv(4096).decode())
PY
```

Expected: 400, main server remains alive. Existing suite covers malformed Host, partial requests, slow clients and traversal attempts. To test an idle incomplete request:

```bash
python3 - <<'PY'
import socket,time
with socket.create_connection(('127.0.0.1',18880),timeout=35) as s:
    s.sendall(b'GET / HTTP/1.1\r\nHost:')
    t=time.monotonic()
    print(repr(s.recv(128)), 'after', round(time.monotonic()-t,1), 'seconds')
PY
```

Observed: `b''` after approximately 29–30 seconds; idle connection is reclaimed. It does not send 408, but this probe tests cleanup rather than an asserted HTTP timeout response.

## G. Port issues — all questions

### 40. Do different interfaces/ports serve the appropriate websites in a browser?

**Browser addresses:** `http://127.0.0.1:18880/index.html`, `http://127.0.0.1:18881/`, `http://127.0.0.2:18880/`.

Expected: `PRIMARY INDEX`, `SECOND SITE`, `SECOND SITE`. HTTP behavior passed; verify the browser manually. IP address controls which local listener receives the connection; port distinguishes services on that address.

### 41. What about multiple websites on the same interface:port?

> “We implement virtual hosting. Server blocks sharing an address and port use one listening socket. The request's Host header selects a server from that listener's candidates; an unknown host uses the default candidate.”

```bash
curl --noproxy '*' -i -H 'Host: primary.test' http://127.0.0.1:18880/index.html
curl --noproxy '*' -i -H 'Host: alternate.test' http://127.0.0.1:18880/
curl --noproxy '*' -i -H 'Host: unknown.test' http://127.0.0.1:18880/index.html
```

Expected/observed: primary, second, primary content, all 200. Same endpoint inside one process is valid for this implementation. This uses HTTP headers and needs no `/etc/hosts` edits.

### 42. What if two webserv processes have overlapping listen endpoints?

Keep the demo running and execute:

```bash
./webserv "$EVAL_DIR/collision.conf"
printf 'Exit code: %s\n' "$?"
curl --noproxy '*' -i http://127.0.0.1:18880/index.html
```

The smaller second configuration is a different file but repeats `127.0.0.1:18880`. Expected/observed: `bind() failed`, exit 1; original process still returns 200. `SO_REUSEADDR` helps restart sockets; it does not permit two independent listeners to own this active identical endpoint here. This is coherent rejection, not a crash.

## H. Siege and stress — all questions

### 43. Is availability above 99.5% for an empty-page GET with -b?

Use this Bash array for an installed Siege:

```bash
SIEGE=(siege)
```

For the audit's temporary copy on this machine, use this array instead:

```bash
SIEGE=(/tmp/webserv-siege-tool/unpacked/usr/bin/siege -R /tmp/webserv-siege-tool/unpacked/etc/siege/siegerc)
```

The temporary copy only exists until `/tmp` is cleared. Otherwise install Siege using question 5. The array preserves each argument when passed to `timeout`.

```bash
timeout --kill-after=5s 45s "${SIEGE[@]}" -b -c 20 -r 100 http://127.0.0.1:18880/empty.html
```

`-b` removes think-time delays; `-c 20` limits simultaneous clients; `-r 100` makes each client repeat 100 times. Expected: 2,000 successful requests, availability >99.5%, no failed transactions, server remains responsive. **Observed:** each of three runs achieved 100%, zero failures. These were short local runs (roughly 0.16–0.19 seconds per run), not proof of indefinite operation.

### 44. Does memory grow indefinitely / are there leaks?

In Terminal B while Terminal A runs repeated stress:

```bash
ps -p "$SERVER_PID" -o pid,rss,vsz,etime,cmd
ls "/proc/$SERVER_PID/fd" | wc -l
```

Repeat before and after several rounds. RSS is resident memory in KiB; VSZ includes virtual address space and is not by itself a leak measure. Expected: memory stabilizes after warm-up and descriptor count returns near baseline. Audit observation: RSS stabilized at 4,196 KiB, descriptors at 6.

For a reproducible Valgrind session, stop the plain demo, then run in Terminal A:

```bash
kill "$SERVER_PID"
wait "$SERVER_PID"
valgrind --leak-check=full --show-leak-kinds=all --track-fds=yes --log-file="$EVAL_DIR/valgrind.log" ./webserv "$EVAL_DIR/audit.conf" > "$EVAL_DIR/valgrind-server.log" 2>&1 &
export SERVER_PID=$!
```

Wait for startup, exercise GET, uploads and CGI using previous commands, then `kill "$SERVER_PID"`, `wait "$SERVER_PID"` and `cat "$EVAL_DIR/valgrind.log"`. Expected: no invalid accesses or lost allocations. Current measured sample: zero such errors/losses, but live resources remained reachable when killed. Do not present a forced-exit report as clean graceful shutdown or a proof about untested CGI-disconnect paths.

### 45. Are there hanging connections?

```bash
ss -tanp '( sport = :18880 or sport = :18881 )'
curl --noproxy '*' -i --max-time 2 http://127.0.0.1:18880/empty.html
```

Expected: new request returns 200, no indefinitely accumulating ESTAB/CLOSE-WAIT connections after clients finish. TIME-WAIT is normal TCP cleanup, not a leaked application connection. The audit's idle-client test closes at approximately 30 seconds. **Known exception:** CGI's blocking wait can stall the event loop; see question 33.

### 46. Can Siege run continuously without restarting the server?

After reviewing the load settings with the evaluator, use a bounded longer check:

```bash
timeout --signal=INT --kill-after=5s 70s "${SIEGE[@]}" -b -c 20 -t 60S http://127.0.0.1:18880/empty.html
```

Expected: high availability throughout, stable memory/descriptors and successful GET afterward. For a supervised indefinite demonstration, omit `-r` and `-t`, monitor the process and use Ctrl-C to stop Siege:

```bash
"${SIEGE[@]}" -b -c 20 http://127.0.0.1:18880/empty.html
```

**Additional measured attempt:** `siege -b -c 20 -t 30s` on isolated port 18980 did not terminate at its requested duration. It stopped making progress, printed no statistics, and ignored SIGINT/SIGTERM; it was eventually killed. A separate curl still returned 200 in 0.000601 seconds. This is **inconclusive endurance evidence**, not a measured availability failure or a passing continuous-load result. The [extended metrics](results/endurance-metrics.json) record memory/descriptors and forced-exit status. The GNU `timeout` wrapper above adds an independent deadline; timeout/killed exit codes do not count as a pass.

The original three-run sample does **not** prove indefinite operation. CGI hang findings prevent an unqualified claim that every workload remains responsive forever.

### 47. How do we agree on fair load parameters?

> “We agree on concurrency and either repetitions or duration before testing. We record the OS, target and empty-page size. Benchmark mode removes delays, so limiting concurrency bounds simultaneous work but does not impose an exact requests-per-second ceiling.”

For a gentler test with a maximum think delay of one second:

```bash
timeout --kill-after=5s 60s "${SIEGE[@]}" -c 10 -d 1 -r 20 http://127.0.0.1:18880/empty.html
```

Expected: 200 transactions, slower arrival than benchmark mode. Do not combine `-b` with `-d` and claim the delay throttles benchmark traffic: `-b` disables that think delay. The audit used a bounded 20-client local workload; evaluator agreement remains a live evaluation step.

## What to fix first

1. Remove forbidden post-I/O errno control-flow checks in CGI according to this rubric; preserve correct success/EOF/error handling.
2. Make child reaping nonblocking, call CGI timeout handling, send an HTTP error on timeout, and verify job/descriptor ownership through stdin closure and client disconnects. `_eraseFdFrom` currently removes the client-to-job map even when only one pipe closes; this deserves a dedicated cleanup review.
3. Pass effective index, autoindex, upload enable/path and error-page configuration into the handler rather than relying on hardcoded defaults.
4. Correct response semantics: 204 has no content/Content-Length, 405 includes Allow, and unimplemented methods use 501. Check deletion's return value before reporting success; `RequestHandler` currently ignores `std::remove` failure.
5. Tighten README formatting and replace stale commands/claims, then rerun both suites and the browser checks.

These are findings and reproduction instructions; no implementation fixes were made as part of the requested test/review.

## Cleanup

Stop only processes you launched and whose PID you recorded:

```bash
kill "$SERVER_PID"
wait "$SERVER_PID"
```

In Terminal B, if you launched the browser demo server:

```bash
kill "$BROWSER_SERVER_PID"
wait "$BROWSER_SERVER_PID"
```

A terminated process can make `wait` return a nonzero status; that is expected here. Fixtures and logs remain under the printed temporary directory for review; remove that specific directory later if desired. Audit evidence copied into `docs/Eval/results` remains available even after `/tmp` cleanup.
