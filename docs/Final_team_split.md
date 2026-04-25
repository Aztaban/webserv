*How to split the project between 3 people
**Phase 0 — Do Together First (1-2 days)
- Config file format (what it looks like, what fields exist)
- Data structures (how a Request, Response, Server, Location will look)
- Coding style and naming conventions
- How the event loop will connect everyone's work

**Martin — Network Core & Event Loop
- Socket creation, bind(), listen(), accept()
- The poll()/epoll() event loop
- Non-blocking I/O management
- Client connection lifecycle (connect → read → write → disconnect)
- Stress testing / keeping server alive

**Diana — HTTP Parsing & Response Building
- Parsing raw bytes into structured requests (request line, headers, body)
- Chunked transfer encoding / Content-Length handling
- Building HTTP responses (status codes, headers, body)
- Default error pages
- Serving static files
- Directory listing
- File uploads (POST)

**Lada — Config Parser & CGI
- Config file parser (server blocks, location blocks, all directives)
- Route matching logic (which location block applies to a given URL)
- CGI execution (fork() + execve(), env variables, stdin/stdout piping)
- HTTP redirects
- DELETE method

*Integration Points to Watch
The moments where your work connects are the riskiest — plan for them explicitly:
A ↔ B: Person A reads raw bytes from the socket and hands them to B's parser. Agree on the interface early (a buffer? a string? a class method?).
B ↔ C: Person B needs the config/route data from C to know what to serve and how. The Location struct is shared.
C ↔ A: CGI uses fork() and pipes — the file descriptors from CGI need to be registered with A's poll() loop.
