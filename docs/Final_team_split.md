# How to split the project between 3 people
## Phase 0 — Do Together First (1-2 days)
- Config file format (what it looks like, what fields exist)
- Data structures (how a Request, Response, Server, Location will look)
- Coding style and naming conventions
- How the event loop will connect everyone's work

## Martin — Network Core & Event Loop
- Socket creation, bind(), listen(), accept()
- The poll()/epoll() event loop
- Non-blocking I/O management
- Client connection lifecycle (connect → read → write → disconnect)
- Stress testing / keeping server alive

## Diana — HTTP Parsing & Response Building, Config Parser 
- Parsing raw bytes into structured requests (request line, headers, body)
- Chunked transfer encoding / Content-Length handling
- Building HTTP responses (status codes, headers, body)
- Default error pages
- Serving static files
- Directory listing
- File uploads (POST)
- DELETE method
- Config file parser (server blocks, location blocks, all directives)
- Route matching logic (which location block applies to a given URL)

## Lada — CGI
- CGI execution (fork() + execve(), env variables, stdin/stdout piping)
- HTTP redirects


## Current Status

## Diana:
- Done: HTTP request parsing, HTTP response building, config file parsing, basic static GET, autoindex, POST upload, DELETE, and config unit tests.
- Done: `ServerConfig` / `LocationConfig` storage and the first parser helpers split into smaller files.
  
- Missing: route matching wired into request handling, host/server selection, custom error pages from config, and full config-driven behavior in `HttpRequestHandler`.
- Missing: final edge cases for config validation, duplicate directives, and location inheritance rules.

# Lada:
- Missing: CGI execution, CGI env setup, stdin/stdout piping, CGI response parsing, and redirect handling.


# Integration Points to Watch
The moments where your work connects are the riskiest — plan for them explicitly:
- A ↔ B: Person A reads raw bytes from the socket and hands them to B's parser. Agree on the interface early (a buffer? a string? a class method?).
- B ↔ C: Person B needs the config/route data from C to know what to serve and how. The Location struct is shared.
- C ↔ A: CGI uses fork() and pipes — the file descriptors from CGI need to be registered with A's poll() loop.
