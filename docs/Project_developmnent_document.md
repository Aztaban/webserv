# Webserv - Project Development Document

## Table of content
- [1. Project goal](#1-project-goal)
- [2. Constraints](#2-constraints)
- [3. Development strategy](#3-development-strategy)
- [4. High-level architecture](#4-high-level-architecture)
- [5. Core classes / responsibilites](#5-core-classes--responsibilities)
- [6. Connection lifecycle design](#6-connection-lifecycle-design)
- [7. Minimal HTTP feature scope for first working version](#7-minimal-http-feature-scope-for-first-working-version)
- [8. Important design decisions to settle early](#8-important-design-decisions-to-settle-early)
- [9. Major risks and how to reduce them](#9-major-risks-and-how-to-reduce-them)
- [10. Recommended data model](#10-recommended-data-model)
- [11. Testing strategy from the beginning](#11-testing-strategy-from-the-beginning)
- [12. Concrete milestone plan](#12-concrete-milestone-plan)
- [13. First draft of team split for 3 people](#13-first-draft-of-team-split-for-3-people)
- [14. Team workflow recommendation](#14-team-workflow-recommendation)
- [15. Definition of done for mandatory part](#15-definition-of-done-for-mandatory-part)
- [16. Recommended documents to maintain during development](#16-recommended-documents-to-maintain-during-development)

## 1. Project goal

Build a non-blocking HTTP server in **C++98** that can:

- serve static files,
- handle at least **GET**, **POST**, **DELETE**,
- parse and apply a configuration file,
- support uploads,
- generate correct HTTP responses and error pages,
- support CGI execution,
- listen on multiple ports,
- stay stable under stress.

## 2. Constraints

### Technical constraints

- Language: **C++98**
- Build: `Makefile` with `all`, `clean`, `fclean`, `re`, no unnecessary relinking
- No external libraries, no Boost
- Must not crash under any circumstances
- Must remain non-blocking at all times
- Must use one poll/equivalent for all socket/pipe I/O
- Must never `read`/`write` on socket-like descriptors without prior readiness from poll/equivalent
- `fork` allowed only for CGI
- Must support at least one CGI type

### Functional requirements

- Config file support
- Multiple listening ports
- Static website serving
- GET, POST, DELETE
- File uploads
- Accurate HTTP status codes
- Default error pages
- Route-based config
- Optional directory listing
- Redirection
- Max body size
- CGI by extension

### Evaluation reality

The project is not only about “it works once”. It will be checked for:

- resilience,
- correct non-blocking behavior,
- comparison with browser/NGINX behavior,
- ability to explain architecture and decisions,
- testing quality.


## 3. Development strategy

Do not start from CGI or full HTTP compliance.

Recommended order:

1. Core server loop
2. Socket setup + accepting clients
3. Incremental request receiving
4. HTTP request parsing
5. Basic static GET
6. Response builder + error handling
7. Configuration parser
8. Route resolution
9. POST upload handling
10. DELETE
11. CGI
12. Stress testing + cleanup + edge cases

Reason: the project becomes manageable only if the event loop and connection lifecycle are stable first.


## 4. High-level architecture

1. **Config subsystem**
    - Parse configuration file
    - Validate directives
    - Store server and location config
2. **Network / event loop subsystem**
    - Create listening sockets
    - Register file descriptors
    - Accept clients
    - Track read/write readiness
    - Manage connection state
3. **HTTP parsing subsystem**
    - Request line parsing
    - Header parsing
    - Body handling
    - Chunked decoding if needed for CGI/body processing
4. **Routing / resolution subsystem**
    - Match request to server block
    - Match request path to location block
    - Resolve filesystem path
    - Determine method authorization, redirect, CGI, index, autoindex, upload rules
5. **Response subsystem**
    - Build status line
    - Build headers
    - Serve file content
    - Generate error responses
    - Handle redirection responses
6. **CGI subsystem**
    - Detect CGI target by extension
    - Prepare environment variables
    - Pipe request body to CGI if needed
    - Read CGI output safely
    - Convert CGI output to HTTP response
7. **Utilities / infrastructure**
    - Logging/debug helpers
    - File/path helpers
    - MIME type helpers
    - String helpers
    - Test configs and demo content


## 5. Core classes / responsibilities

### Config side

- `ConfigParser`
    - tokenize config file
    - parse server/location blocks
    - validate syntax
- `ServerConfig`
    - host/port(s), server name, error pages, max body size
- `LocationConfig`
    - path, root, allowed methods, redirect, autoindex, index, upload settings, CGI settings

### Networking side

- `WebServer`
    - owns the whole application lifecycle
- `EventLoop`
    - poll/select/epoll/kqueue wrapper
- `Listener`
    - listening socket abstraction
- `ClientConnection`
    - state of one connected client
    - request buffer, parsed request, response buffer, timestamps, current state

### HTTP side

- `HttpRequest`
    - method, path, query string, version, headers, body
- `HttpRequestParser`
    - incremental parsing from raw bytes
- `HttpResponse`
    - status, headers, body or file target
- `HttpResponseBuilder`
    - factory/helper to create standard responses
- `Router`
    - maps request to effective server/location config
- `ResourceResolver`
    - filesystem mapping, index selection, autoindex, upload path logic

### CGI side

- `CgiHandler`
    - prepares execution
    - builds env
    - pipes body
    - reads output
    - enforces non-blocking behavior for related descriptors

### Utility side

- `MimeTypes`
- `PathUtils`
- `StringUtils`
- `StatusCode`
- `Logger`


## 6. Connection lifecycle design

1. Socket becomes readable
2. Accept new client
3. Client fd registered for read/write readiness tracking
4. Receive bytes incrementally
5. Detect whether full headers are available
6. Parse headers
7. Validate body expectations (Content-Length, transfer mode, method rules, body size)
8. Continue receiving until full request is available
9. Route the request
10. Produce response:
    - static file
    - autoindex
    - upload handling
    - delete handling
    - CGI
    - redirect
    - error page
11. Mark client ready to write
12. Send response incrementally
13. Close or keep-alive according to chosen behavior/support level
14. Clean up fd and memory safely


## 7. Minimal HTTP feature scope for first working version

### Milestone A — “server lives”
- start server
- listen on one port
- accept multiple clients
- read request
- return simple fixed response

### Milestone B — “basic HTTP”
- parse request line + headers
- serve static file for GET
- return 404 / 405 / 500 correctly
- basic config load

### Milestone C — “project baseline complete”
- multiple servers/ports
- route config
- POST upload
- DELETE
- error pages
- index file
- autoindex

### Milestone D — “evaluation ready”
- CGI
- robust invalid request handling
- stress testing
- browser checks
- NGINX comparison
- documentation cleanup


## 8. Important design decisions to settle early

These should be agreed by the team before heavy coding:

### 1. Event mechanism

Choose one:
- `poll()` for simplicity and subject familiarity
- `kqueue` / `epoll` only if you all understand it well

For a 42 project, **poll() is usually the safest common choice** unless your team is already very comfortable with platform-specific APIs.

### 2. Parsing style

Prefer **incremental parsing** over “read everything and then parse”. This matters because requests may arrive in chunks.

### 3. Request body policy

Decide exactly how body collection works:
- only after headers are parsed,
- track expected size,
- reject oversized body early,
- define timeout/cleanup behavior for incomplete requests.

### 4. Keep-alive policy

You can simplify early development by closing each connection after one response, then add persistent behavior later only if needed.

### 5. CGI integration strategy

Clearly separate:
- request preparation,
- process launch,
- body streaming,
- CGI output parsing,
- final HTTP response creation.

### 6. Path safety

Prevent broken path resolution and traversal issues:
- normalize paths,
- check route root joins carefully,
- validate upload target directories.


## 9. Major risks and how to reduce them

### Risk: event loop becomes messy

`Mitigation:` define client states explicitly. Example states:
- `READING_HEADERS`
- `READING_BODY`
- `PROCESSING`
- `WRITING_RESPONSE`
- `CGI_RUNNING`
- `DONE`
- `ERROR`

### Risk: parser bugs everywhere

`Mitigation:` isolate request parsing from socket code. Feed parser raw strings/chunks and test it independently.

### Risk: CGI blocks server

`Mitigation:` CGI pipes and communication must follow the same non-blocking discipline where applicable. Keep CGI logic encapsulated.

### Risk: config parser eats too much time

`Mitigation:` keep grammar intentionally small and close to project requirements only. Do not over-design it into a full nginx clone.

### Risk: team members build incompatible pieces

`Mitigation:` define shared interfaces and data structures before parallel work.

### Risk: passes your tests but fails evaluation

`Mitigation:` compare against browser behavior, curl, telnet, and NGINX regularly.


## 10. Recommended data model

**Request**
- method
- raw target
- normalized path
- query string
- http version
- headers map
- body
- content length
- is chunked
- boundary/multipart info if needed

**Response**
- status code
- reason phrase
- headers map
- body string OR file descriptor/path
- auto-close flag

**Client connection**
- fd
- current state
- input buffer
- output buffer
- parsed request
- selected server config
- selected location config
- bytes sent
- last activity time
- CGI metadata if active

## 11. Testing strategy from the beginning

Testing cannot be postponed to the end.

### Manual tools

- browser
- `curl`
- `telnet` / `nc`
- compare with `nginx`

### Automated tests to write

1. **Parser tests**
    - valid request line
    - invalid method/version
    - malformed headers
    - body parsing
    - oversized body
2. **Config tests**
    - valid blocks
    - missing values
    - duplicate/contradictory directives
    - route inheritance/override rules
3. **HTTP behavior tests**
    - GET existing file
    - GET missing file → 404
    - POST upload success/failure
    - DELETE existing/missing target
    - unsupported method → 405
    - redirect
    - autoindex on/off
4. **CGI tests**
    - simple GET CGI
    - POST CGI with body
    - CGI without content-length
    - CGI execution failure
5. **Stress / resilience tests**
    - many quick connections
    - partial request sending
    - disconnect during request
    - large body near limit
    - invalid repeated requests

### Useful principle

Every bug found during manual testing should produce a reproducible test case.


## 12. Concrete milestone plan

### Phase 1 — Foundation
- choose event API
- define core classes and headers
- define config grammar
- create project skeleton
- implement listening socket creation
- implement accept + connection storage

### Phase 2 — Request/response core
- implement read buffer management
- implement request parser
- implement minimal response builder
- return static hardcoded responses
- then static file responses

### Phase 3 — Config and routing
- parse server/location config
- route matching
- allowed methods
- root/index/error page resolution
- redirects
- autoindex

### Phase 4 — Write operations
- POST upload
- DELETE
- max body size
- failure cases

### Phase 5 — CGI
- extension mapping
- env creation
- fork/execve
- pipe integration
- parse CGI output

### Phase 6 — Hardening
- timeouts / stale clients handling
- disconnect safety
- memory/resource cleanup
- stress testing
- compare with NGINX
- README and evaluator demo prep


## 13. First draft of team split for 3 people

### Person 1 — Core networking / event loop
- listening sockets
- poll loop
- fd registration/removal
- client connection state machine
- non-blocking read/write
- connection cleanup

### Person 2 — HTTP + config
- config parser
- server/location models
- request parser
- route matching
- response builder
- status codes / error pages

### Person 3 — Filesystem + methods + CGI
- static file serving
- upload handling
- delete handling
- directory listing
- CGI execution and parsing
- integration tests for method behaviors

### Shared responsibilities

All three should understand:
- request lifecycle,
- config format,
- event loop rules,
- testing process.

Nobody should own a subsystem that only they can explain.


## 14. Team workflow recommendation

- Keep one shared architecture note updated.
- Decide coding style and naming early.
- Merge small, often.
- Do not let one branch drift for too long.
- Review each other’s code in terms of explanation, not only “it compiles”.
- Maintain one shared checklist of subject requirements.

Suggested recurring team routine:
- short sync: what was done, blockers, next integration point
- quick demo after each milestone
- add tests before moving to next big subsystem


## 15. Definition of done for mandatory part

You are ready for evaluation only when you can demonstrate:
- multiple ports working,
- static site working in browser,
- GET/POST/DELETE working,
- upload works,
- default/custom error pages work,
- route config works,
- redirect works,
- autoindex/index behavior works,
- CGI works,
- server stays alive under stress and bad input,
- every teammate can explain the architecture.


## 16. Recommended documents to maintain during development

Create these early in `docs/`:

`**architecture.md**`

- components
- ownership
- interactions

`**request_lifecycle.md**`

- exact flow from accept to response close

`**config_reference.md**`

- supported directives
- syntax examples
- defaults

`**testing_checklist.md**`

- required tests
- pass/fail log

`**team_split.md**`

- current ownership
- integration boundaries
- backup owner for each subsystem

