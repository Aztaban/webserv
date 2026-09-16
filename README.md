*This project has been created as part of the 42 curriculum by **mjusta**, **lformnak**, **dkolarov**.*

# Webserv

## Description

**Webserv** is a custom HTTP server written from scratch in **C++98**.

The goal of the project is to understand how a web server works internally, not by wrapping an existing library, but by implementing the core mechanics ourselves: socket handling, non-blocking I/O around a single event loop, HTTP request parsing, response generation, and CGI execution. The server is configured through a file whose syntax is loosely inspired by NGINX, allowing multiple virtual hosts, routes, and behaviors to be defined without touching the code.

At a high level, the server:
- Accepts and manages **multiple simultaneous client connections** using non blocking sockets and a single `poll()`-based event loop (no threads, no fork per connection, no blocking I/O outside the loop)
- Parses raw HTTP/1.1 requests (method, path, headers, body)
- Handles `GET`, `POST`, and `DELETE`
- Serves static files, including index resolution and custom error pages
- Supports file **uploads**
- Executes **CGI** scripts and returns their output as a valid HTTP response
- Supports **multiple ports and virtual hosts** driven entirely by the configuration file

This project follows the 42 Webserv subject requirements strictly, in particular: never blocking on a read/write outside of the poll mechanism, and never checking `errno` after a `read`/`recv`/`write`/`send` to decide whether the connection is closed.

---

## Instructions

### Requirements
- A C++98-compliant compiler (`c++`, `g++`, or `clang++`)
- `make`
- Linux or macOS (any POSIX environment with `poll`, `select`, `epoll`, or `kqueue` support)

### Compilation

```bash
git clone <repository_url>
cd webserv
make
```

This produces a `webserv` executable at the project root. Other available targets:

```bash
make clean     # remove object files
make fclean    # remove object files and the binary
make re        # fclean + full rebuild
```

### Running the server

```bash
./webserv [path/to/config_file]
```

If no configuration file is given, the server falls back to a default configuration (`conf/default.conf`).

On startup the server:
1. Parses the configuration file
2. Opens one listening socket per configured `host:port`
3. Enters the event loop
4. Waits for and serves client connections

### Sending requests

Once running, the server can be tested with a browser or from the terminal:

```bash
curl -i http://localhost:8080/
curl -i http://localhost:8080/about.html
curl -i -X POST --data-binary @file.txt http://localhost:8080/upload
curl -i -X DELETE http://localhost:8080/uploads/file.txt
```

### Request lifecycle

1. Client connects
2. Server accepts the connection (non-blocking)
3. Server reads the request as data becomes available
4. Request is parsed into method, path, headers, and body
5. The route is resolved from the configuration (virtual host + longest prefix location match)
6. A handler is selected: static file, upload, delete, redirect, CGI, or error
7. A response is built (status line, headers, body)
8. The response is sent back to the client
9. The connection is closed or kept alive, depending on the request

### Configuration file

The configuration file defines everything the server does at runtime no behavior is hardcoded. Example:

```nginx
server {
    listen 127.0.0.1:8080;

    root ./www/site1;
    index index.html;

    client_max_body_size 1000000;

    error_page 404 ./www/site1/errors/404.html;

    location / {
        methods GET;
        root ./www/site1;
        index index.html;
        autoindex off;
    }

    location /upload {
        methods GET POST;
        root ./www/site1;
        upload on;
        upload_path ./www/site1/uploads;
    }

    location /uploads {
        methods GET DELETE;
        root ./www/site1/uploads;
        autoindex on;
    }

    location /cgi-bin {
        methods GET POST;
        root ./cgi-bin;
        cgi_extension .py /usr/bin/python3;
    }
}
```

Multiple `server` blocks can be defined in the same file to serve several virtual hosts and/or ports at once.

### Testing

- **Manual:** the `curl` commands above, plus a browser for visual checks (static pages, uploads, error pages)
- **Automated:** `tests/` contains a script that runs the full request/response validation suite:
```bash
  ./tests/run_tests.sh
```
- **Stress testing:** the server was tested under concurrent connections and slow/partial clients to confirm it never blocks or crashes (e.g. using `siege`, `ab`, or a custom slow-client script)

See [`docs/TESTING.md`](docs/TESTING.md) for the full test checklist with commands, expected results, and an explanation of how the server handles each case.

---

## Project Structure

```
webserv/
├── conf/          # configuration files (default.conf, examples)
├── www/           # served static content (per virtual host)
├── cgi-bin/       # CGI scripts
├── include/       # header files
├── src/
│   ├── config/    # configuration parsing (ConfigParser, ServerConfig, LocationConfig)
│   ├── network/   # sockets, event loop, client connections
│   ├── http/      # request parsing, response building
│   ├── router/    # host/location matching
│   ├── cgi/       # CGI process handling
│   └── utils/     # logging, file utilities
├── docs/          # internal documentation (architecture, testing, evaluation guide)
└── tests/         # manual and automated test scripts
```

## Features

- [x] Single-event-loop, non-blocking I/O (`poll`/`select`/`epoll`)
- [x] Multiple simultaneous client connections
- [x] Basic HTTP/1.1 support
- [x] `GET`, `POST`, `DELETE` methods
- [x] Static file serving with index resolution
- [x] File uploads
- [x] File deletion
- [x] CGI execution (configurable interpreter per extension)
- [x] Multiple ports and virtual hosts
- [x] Per-route method restrictions
- [x] Custom error pages
- [x] Redirects
- [x] Configurable maximum request body size

## Technical Choices

- **C++98** throughout, per the subject constraints.
- A **single `poll()`-based event loop** rather than threads or a process per client model, to satisfy the "no blocking I/O outside the loop" requirement and avoid concurrency bugs (locks, races) entirely.
- Configuration syntax deliberately mirrors **NGINX** conventions so the behavior is easy to reason about and verify against a well-known reference server NGINX itself is not used anywhere in the implementation.
- Clear separation of concerns across modules (config / network / http / router / cgi), so each subsystem can be tested and defended independently.

---

## Resources

**Documentation and references used:**
- [RFC 7230 — HTTP/1.1: Message Syntax and Routing](https://datatracker.ietf.org/doc/html/rfc7230)
- [RFC 7231 — HTTP/1.1: Semantics and Content](https://datatracker.ietf.org/doc/html/rfc7231)
- [RFC 3875 — The Common Gateway Interface (CGI)](https://datatracker.ietf.org/doc/html/rfc3875)
- [NGINX documentation](https://nginx.org/en/docs/) — used as a reference for configuration syntax and server behavior, not as implementation source
- `man poll`, `man select`, `man socket`, `man fork`, `man execve` POSIX manual pages for the system calls at the core of the project
- 42 Webserv subject PDF

**How AI was used:**
- Structuring and drafting the Theorical parts and structuring the basic documentation (architecture walkthrough, testing checklists, evaluation/defense guide)
- Proofreading and improving clarity/English phrasing in written documentation
- Helping to debug faild cases
- Helping to generate an extra .py test script

All C++ implementation networking, parsing, routing, CGI handling, and configuration logic was designed and written by the team members listed above. No AI-generated code was integrated into the server itself.

---

## Notes

- The server must never block indefinitely or crash on malformed input, slow clients, or invalid configuration this was a core design constraint, not an afterthought.
- NGINX is referenced only for configuration-syntax inspiration; it is not a dependency and is not used at runtime.