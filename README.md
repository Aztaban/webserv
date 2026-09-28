*This project has been created as part of the 42 curriculum by mjusta, lformank, dkolarov.*

# Webserv

## Description

Webserv is an HTTP/1.1 server written in C++98. We built it to learn how TCP connections, HTTP parsing, routing, file handling and CGI execution work together.

Configuration defines listening addresses, virtual hosts and locations. One `poll()` loop manages nonblocking listening/client sockets and CGI pipes. Requests select a virtual host and the longest matching location, then serve a file, handle an upload or deletion, redirect, or launch a CGI process. Responses are buffered and sent as the client socket becomes writable. Each connection closes after its response; persistent connections are not implemented.

The implementation is divided into configuration, networking, HTTP, routing, CGI and utility modules. Regular-file access is synchronous. CGI runs in a child process; the parent collects its output without blocking on child exit and enforces a 30-second timeout.

## Instructions

### Build and run

Run these commands from the repository root on Linux, with a C++98 compiler and `make` installed. Python 3 is needed for the test suite and Python CGI examples.

```bash
make
./webserv conf/default.conf > /tmp/webserv.log 2>&1
```

The default site listens on **127.0.0.1:8001**. Omitting the configuration argument also selects `conf/default.conf`. Stop the foreground server with Ctrl-C. Relative configuration paths are resolved from the process working directory.

Diagnostics are emitted only when stdout/stderr point to regular files. Terminal, pipe and socket output is suppressed to avoid unpolled logging on event-driven descriptors. Use the redirection above to retain startup and error messages; inspect them from another terminal with `tail /tmp/webserv.log`. Exit status still signals startup failure.

```bash
make clean   # remove object files
make fclean  # remove object files and executables
make re      # clean rebuild
```

A second `make` after a successful build should do no work.

### Try the default site

Keep the server running in the first terminal. In a second terminal, from the repository root:

```bash
curl -i http://127.0.0.1:8001/                         # 200: index page
curl -i http://127.0.0.1:8001/listing/                 # 200: directory listing
curl -i http://127.0.0.1:8001/old                      # 301: Location: /
curl -i http://127.0.0.1:8001/missing                  # 404: error page
printf 'upload demonstration\n' > /tmp/webserv-demo.txt
mkdir -p www/site1/uploads
curl -i --data-binary @/tmp/webserv-demo.txt http://127.0.0.1:8001/upload/demo.txt
curl -i http://127.0.0.1:8001/uploads/demo.txt
curl -i -X DELETE http://127.0.0.1:8001/uploads/demo.txt
```

The last three requests should return 201, 200 with the uploaded bytes, and 204 with no body. Use a filename that does not already exist: uploads overwrite their destination. Failed writes return 500 but can leave a partial file.

### Configuration

See [conf/default.conf](conf/default.conf) for a complete example. Supported directives include:

| Scope | Directives |
| --- | --- |
| Server | `listen`, `server_name`, `root`, `index`, `client_max_body_size`, `error_page` |
| Location | `methods`, `root`, `index`, `autoindex`, `return`, `upload`, `upload_path`, `cgi_extension` |

Multiple server blocks can share an address/port using virtual hosts. Multiple distinct listeners are also supported. A second process cannot bind an address/port already in use.

A location index overrides the server index. Directory listing requires `autoindex on` when no index file exists. File-upload POST requires `upload on`; its destination is `upload_path`, or the effective root's `uploads` directory when omitted. The destination must exist. CGI POST is independent of file-upload permission. The implemented methods are GET, POST and DELETE: unsupported methods return 501; route-disallowed methods return 405 with `Allow`.

### Tests and evaluation

```bash
python3 tests/runner.py          # complete regression suite
python3 tests/runner.py unit     # C++ unit tests
python3 tests/runner.py network  # networking tests
python3 tests/runner.py e2e      # HTTP integration tests
python3 docs/Eval/evaluation_audit.py
```

Run tests as an unprivileged user; permission regressions require it. The runner uses ports 18080/18081 and writes `tests/report.html`. The audit uses loopback ports 18880/18881, creates temporary fixtures and reports their path. It uses Siege if installed; unavailable Siege is reported as skipped. Do not run overlapping suites on the same ports.

The [evaluation guide](docs/Eval/EVALUATION_GUIDE.md) contains theory, demonstration commands, fix history and evidence. Historical audit failures are labelled as such. Finite tests cannot prove indefinite stability or replace the evaluator's interactive browser checks.

## Project structure

- `src/config`, `include/config`: tokenizer, parser and configuration models.
- `src/network`, `include/network`: listeners, client state and event loop.
- `src/http`, `include/http`: request parsing, file handling and response serialization.
- `src/routing`, `src/core`: host/location selection and request dispatch.
- `src/cgi`, `include/cgi`: child processes, environment, pipes and CGI responses.
- `src/utils`: logging and file helpers.
- `conf`, `www`, `cgi-bin`: example configuration, static sites and scripts.
- `tests`, `docs/Eval`: regression tests and evaluation material.

## Resources

- [RFC 9110 — HTTP Semantics](https://www.rfc-editor.org/rfc/rfc9110)
- [RFC 9112 — HTTP/1.1](https://www.rfc-editor.org/rfc/rfc9112)
- [RFC 3875 — CGI](https://www.rfc-editor.org/rfc/rfc3875)
- [NGINX documentation](https://nginx.org/en/docs/) for configuration concepts; NGINX is not a runtime dependency.
- Local manual pages: `man poll`, `man socket`, `man accept`, `man fork`, `man execve`, `man waitpid`.
- The 42 Webserv subject and supplied evaluation checklist.

### AI use

AI assisted with documentation and theory explanations, evaluation planning, regression tests, diagnosis and C++ fixes. Assisted implementation areas include CGI I/O/lifecycle/timeouts and descriptor cleanup; configured indexes, listings, uploads and error pages; HTTP status/header handling; file-operation failure checks; listener readiness; and logging. The team must review and understand these changes before defending the project. The original claim that no AI-generated server code was integrated is no longer accurate.
