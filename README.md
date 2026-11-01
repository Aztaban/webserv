*This project has been created as part of the 42 curriculum by mjusta, lformank, dkolarov.*

# Webserv

A 42 project focused on building an **HTTP/1.1 server in C++98**. The goal is to understand how a web server works: accepting TCP connections, parsing HTTP requests, routing URLs, serving files, handling uploads, and executing CGI scripts.

## 🚀 Overview

This webserv implements:

- Nonblocking sockets managed by a single **`poll()` event loop**
- HTTP request parsing and response generation
- **GET**, **POST**, and **DELETE** methods
- Static files, configurable index pages, and directory listings
- File uploads and deletion
- **CGI execution** with Python examples
- Multiple listening ports and name-based virtual hosts
- Configurable routes, redirects, body-size limits, and custom error pages
- Our own **Python test suite** with an HTML results report

## ✅ Supported features

### HTTP / Networking

- Multiple clients handled through readiness-based socket I/O
- Partial request reads and partial response writes
- Request bodies using `Content-Length` or chunked transfer encoding
- Request-body size limits with `413 Payload Too Large` responses
- Method restrictions per location: `405 Method Not Allowed` with an `Allow` header
- Unsupported methods return `501 Not Implemented`

### Files / Routes

- **GET**: serve static files and directory index pages
- **POST**: save an uploaded request body when uploads are enabled
- **DELETE**: remove files when allowed by the route
- Directory listings with `autoindex on`
- Redirects using the `return` directive
- Custom error pages configured per server
- Virtual-host selection and longest matching location selection

### CGI

- Interpreter mapping by extension, such as `.py` → `/usr/bin/python3`
- Child processes launched with `fork()` and `execve()`
- CGI environment variables and request-body input through stdin
- CGI output collected through pipes monitored by the event loop
- A **30-second timeout** for CGI processes
- Example scripts in `cgi-bin/`

## 📁 Folder structure

```text
├── cgi-bin/        # Example Python CGI scripts
├── conf/           # Server configuration examples
├── docs/           # Architecture, request lifecycle, and evaluation guides
├── include/        # C++ headers, grouped by module
├── src/
│   ├── cgi/        # CGI processes, environment, pipes, and output parsing
│   ├── config/     # Configuration tokenizer, parser, and models
│   ├── core/       # Server setup, request dispatch, and error responses
│   ├── http/       # HTTP parsing, file handling, and response building
│   ├── network/    # Listeners, client connections, and poll event loop
│   ├── routing/    # Virtual-host and location selection
│   ├── utils/      # Logging and file helpers
│   └── main.cpp    # Program entry point
├── tests/
│   ├── e2e/        # Python tests using real HTTP requests
│   ├── network/    # Python socket and connection tests
│   ├── unit/       # C++ unit tests
│   ├── fixtures/   # Test configuration and data
│   ├── lib/        # HTTP client, assertions, context, and reporting helpers
│   └── runner.py   # Custom test runner
├── www/            # Demo sites, assets, uploads, and error pages
└── Makefile        # Build rules
```

## 🧠 Request flow (high level)

### 1. Load configuration

Parses the server and location blocks, validates settings, and creates the configured listening sockets.

### 2. Accept connections

The `poll()` loop watches listening sockets, client sockets, and CGI pipes. New clients get their own connection state and buffers.

### 3. Parse the request

Reads available bytes and parses the request line, headers, and body. Incomplete requests wait for more data; invalid requests receive an error response.

### 4. Select a route

Chooses the virtual host and longest matching location, then applies method permissions, body limits, and route settings.

### 5. Handle the request

Serves a file, generates a directory listing, saves an upload, deletes a file, returns a redirect, or starts a CGI process.

### 6. Send the response

Builds the status line, headers, and body. Buffered output is sent as the socket becomes writable, then the connection closes.

## 🛠️ Compilation

Build from the repository root on **Linux**, with a C++98 compiler and `make` installed. Python 3 is required for the custom tests and Python CGI examples.

```bash
make            # builds webserv
make clean      # removes object files
make fclean     # removes binaries and objects
make re         # rebuilds everything
```

The project compiles with `-Wall -Wextra -Werror -std=c++98`.

## 💻 Usage

### Run

```bash
./webserv conf/default.conf > /tmp/webserv.log 2>&1
```

Open **http://127.0.0.1:8001/** in your browser. Omitting the configuration argument also selects `conf/default.conf`. Stop the foreground server with `Ctrl+C`.

Run from the repository root so relative configuration paths resolve correctly. Diagnostics are written only when stdout/stderr point to regular files; redirect them as above and inspect them from another terminal:

```bash
tail -f /tmp/webserv.log
```

### Examples

With the default server running, use a second terminal:

```bash
curl -i http://127.0.0.1:8001/                         # index page
curl -i http://127.0.0.1:8001/listing/                 # directory listing
curl -i http://127.0.0.1:8001/old                      # 301 redirect to /
curl -i http://127.0.0.1:8001/missing                  # 404 error page
curl -i 'http://127.0.0.1:8001/cgi-bin/hello.py?name=Webserv'
```

### Upload and delete

Run these commands from the repository root. Choose a filename that does not already exist: uploads overwrite their destination.

```bash
printf 'Hello from Webserv!\n' > /tmp/webserv-demo.txt
mkdir -p www/site1/uploads

# Upload a raw request body → 201 Created
curl -i --data-binary @/tmp/webserv-demo.txt http://127.0.0.1:8001/upload/demo.txt

# Read the uploaded file → 200 OK
curl -i http://127.0.0.1:8001/uploads/demo.txt

# Delete the file → 204 No Content
curl -i -X DELETE http://127.0.0.1:8001/uploads/demo.txt
```

## ⚙️ Configuration

Configuration uses `server` and `location` blocks. See [conf/default.conf](conf/default.conf) for a complete example.

| Scope | Supported directives |
| --- | --- |
| Server | `listen`, `server_name`, `root`, `index`, `client_max_body_size`, `error_page` |
| Location | `methods`, `root`, `index`, `autoindex`, `return`, `upload`, `upload_path`, `cgi_extension` |

- Multiple server blocks can share an address/port through virtual hosts or use distinct listeners.
- A location's `index` overrides the server-level index.
- Directory listing requires `autoindex on` when no index file exists.
- File-upload POST requires `upload on`. The destination is `upload_path`, or the effective root's `uploads` directory when omitted. The destination directory must exist.
- CGI POST works independently of file-upload permission.

Additional examples cover [multiple ports](conf/multi_port.conf), [static files](conf/static_only.conf), [uploads and deletion](conf/upload_delete.conf), [CGI](conf/cgi.conf), and [small body limits](conf/small_body_limit.conf).

## 🧪 Custom Python tests

We built our own test suite in **Python**, with reusable HTTP helpers, assertions, test fixtures, and HTML reporting. It sends real requests to the server, including raw socket requests for protocol and networking checks.

The runner builds the project, starts the server when needed, runs the selected tests, writes a report, and stops the server afterward. It also compiles and executes the registered **C++ unit tests**.

### Run the tests

```bash
python3 tests/runner.py          # full regression suite
python3 tests/runner.py network  # socket and connection tests
python3 tests/runner.py e2e      # HTTP end-to-end tests
python3 tests/runner.py unit     # registered C++ unit tests
```

### Coverage

- Connections, disconnects, slow clients, concurrent clients, and multiple ports
- Static files, browser assets, configured indexes, and directory listings
- Uploads, upload policies, deletion, and filesystem failures
- Routing, redirects, method restrictions, and custom error pages
- CGI requests, process lifecycle, timeouts, and resource cleanup
- HTTP parsing, response building, and injected CGI startup failures in C++ tests

Results are written to **`tests/report.html`**, with server diagnostics in `tests/server_test.log`. The test server uses loopback ports **18080 / 18081**. Run as an unprivileged user for permission-related checks, and avoid overlapping suites on the same ports.

### Evaluation checks

```bash
python3 docs/Eval/evaluation_audit.py
```

The audit uses ports **18880 / 18881** and temporary fixtures. It also runs Siege checks when Siege is installed; otherwise, those checks are skipped. See the [evaluation guide](docs/Eval/EVALUATION_GUIDE.md) for demonstration commands and recorded results.

## 📝 Notes / Limitations

- This project targets the 42 Webserv requirements.
- Each connection closes after its response; persistent connections are not implemented.
- Regular-file access is synchronous; sockets and CGI pipes are handled through the event loop.
- Failed upload writes return `500` and can leave a partial file.
- Automated tests cover regressions but do not replace interactive browser checks or prove indefinite stability.

## 📚 Resources

- [RFC 9110 — HTTP Semantics](https://www.rfc-editor.org/rfc/rfc9110)
- [RFC 9112 — HTTP/1.1](https://www.rfc-editor.org/rfc/rfc9112)
- [RFC 3875 — CGI](https://www.rfc-editor.org/rfc/rfc3875)
- [NGINX documentation](https://nginx.org/en/docs/) for configuration concepts; NGINX is not a runtime dependency.
- Local manual pages: `man poll`, `man socket`, `man accept`, `man fork`, `man execve`, `man waitpid`.
- The 42 Webserv subject and supplied evaluation checklist.
- Project documentation: [architecture](docs/Architecture.md) and [request lifecycle](docs/Request_lifecycle.md).

## 👥 Authors

Created and maintained by **[Martin Justa](https://github.com/aztaban)**, **[Diana Kolarova](https://github.com/dkolarova)**, and **[Lada Formankova](https://github.com/lformank)** as part of the 42 school curriculum.
