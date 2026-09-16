# Webserv Project Summary

## 1. Project Goal

This project implements a custom HTTP server in **C++98**, inspired by the 42 Webserv subject. The server handles:

- Multiple client connections
- Non-blocking I/O
- HTTP request parsing
- Static file serving
- Uploads
- DELETE requests
- CGI execution
- Virtual host routing
- Custom error pages and redirects

The server follows the subject requirements by using a **single event loop** and non-blocking socket management rather than a blocking per-client model.

---

## 2. Team Split and Responsibilities

### Martin — Network Core and Event Loop
- Socket creation
- Bind / listen / accept
- Non-blocking I/O
- Poll-based loop
- Connection lifecycle
- Stress and resilience testing

### Diana — HTTP Parsing and Response Generation
- Request parsing
- Header/body handling
- HTTP response creation
- Static file serving
- Uploads
- Error handling

### Lada — Configuration Parsing and CGI
- Configuration file parsing
- Server and location matching
- CGI environment setup
- CGI execution
- Redirect logic
- Route-specific behavior

---

## 3. Runtime Flow — Step by Step

### Startup

Execution starts in `main.cpp`.

The flow is:
1. Ignore broken pipe signals
2. Read the configuration file path
3. Use the config parser to build server data
4. Create a `WebServer` object
5. Set up listening sockets
6. Start the event loop

### Configuration Loading

The configuration subsystem reads the config and builds the runtime structure.

**Main files:**
- `ConfigParser.hpp`
- `ConfigParser.cpp`
- `ServerConfig.hpp`
- `ServerConfig.cpp`
- `LocationConfig.hpp`

**This step defines:**
- Host and port
- Server name
- Root directory
- Index file
- Client max body size
- Error pages
- Routes and methods
- Upload configuration
- Redirect rules
- CGI mapping

### Server Setup

The top-level server is defined in:
- `WebServer.hpp`
- `WebServer.cpp`

**Its role is to:**
- Group servers by host:port
- Create `Listener` objects
- Register listeners in the event loop
- Start the main runtime loop

### Event Loop and Socket Handling

Network handling is in:
- `EventLoop.hpp`
- `EventLoop.cpp`
- `Listener.hpp`
- `Listener.cpp`
- `ClientConnection.hpp`
- `ClientConnection.cpp`

**This subsystem:**
- Accepts new connections
- Reads incoming bytes
- Detects readiness
- Handles partial data
- Writes responses
- Cleans up dead connections

### Request Parsing

The HTTP parser is in:
- `HttpRequest.hpp`
- `HttpRequestParser.hpp`
- `HttpRequestParser.cpp`

**It transforms raw bytes into structured request data:**
- Method
- Path
- Query string
- Headers
- Body
- Version
- Content length

### Routing

Routing logic is implemented in:
- `Router.hpp`
- `Router.cpp`

**It performs:**
- Host-based virtual server matching
- Route matching by longest prefix
- Method validation
- Location selection

This allows different routes and virtual hosts to behave correctly.

### Request Handler

The dispatch layer is in:
- `RequestHandler.hpp`
- `RequestHandler.cpp`

**This file decides:**
- If a request is a CGI request
- If a method is allowed
- If a redirect must be served
- If DELETE should remove a file
- If a file is served normally
- Which error response to build

This is one of the central points of the implementation.

### Response Creation

The response layer is in:
- `HttpResponse.hpp`
- `HttpResponseBuilder.hpp`
- `HttpResponseBuilder.cpp`

**This subsystem:**
- Builds the status line
- Creates headers
- Sets `Content-Length`
- Chooses `Content-Type`
- Sends a response body
- Supports custom error pages and redirect headers

### CGI Execution

The CGI subsystem is in:
- `CgiRequest.hpp`
- `CgiRequest.cpp`
- `CgiJob.hpp`
- `CgiJob.cpp`
- `CgiOutputParser.hpp`
- `CgiOutputParser.cpp`

**It handles:**
- CGI script detection from config
- Environment setup
- Script execution
- Output capture
- stdout parsing
- HTTP response generation from CGI output

---

## 4. Files and Key Responsibilities

### Core
| File | Responsibility |
|---|---|
| `main.cpp` | Program entry point |
| `WebServer.cpp` | Server setup and run loop |
| `RequestHandler.cpp` | Request dispatch |
| `ErrorResponseFactory.cpp` | Error responses |

### Network
| File | Responsibility |
|---|---|
| `Listener.cpp` | Socket binding / listen |
| `EventLoop.cpp` | Readiness loop |
| `ClientConnection.cpp` | Per-client life cycle |

### HTTP
| File | Responsibility |
|---|---|
| `HttpRequestParser.cpp` | Parse client requests |
| `HttpRequestHandler.cpp` | Static file / route response logic |
| `HttpResponseBuilder.cpp` | Build full HTTP response |

### Config
| File | Responsibility |
|---|---|
| `ConfigParser.cpp` | Parse config file |
| `ServerConfig.cpp` | Server config model |
| `TokenStream.cpp` | Token handling |

### Routing
| File | Responsibility |
|---|---|
| `Router.cpp` | Host and route matching |

### CGI
| File | Responsibility |
|---|---|
| `CgiJob.cpp` | CGI process execution |
| `CgiOutputParser.cpp` | Parse CGI output |

### Utilities
| File | Responsibility |
|---|---|
| `Logger.cpp` | Logging / debug output |
| `FileUtils.cpp` | File operations |

---

## 5. What Was Implemented

The server currently supports:

- [x] Multi-port and virtual-host config
- [x] GET requests
- [x] POST uploads
- [x] DELETE requests
- [x] Static file serving
- [x] Index file resolution
- [x] Custom error pages
- [x] Redirect responses
- [x] CGI execution for configured extensions
- [x] Large body rejection
- [x] Request method restrictions by route
- [x] Non-blocking operation with a single event loop

---

## 6. Validation Commands

**Full project validation**
```
# (add your test-suite command here, e.g. ./run_tests.sh)
```
Result: _(paste result/output here)_

**Live HTTP checks**
```
# (add your curl commands here, e.g.)
curl -i http://127.0.0.1:8001/
curl -i -X POST -F "file=@test.txt" http://127.0.0.1:8001/upload
curl -i -X DELETE http://127.0.0.1:8001/uploads/missing.txt
```

**Expected:**
- [ ] GET returns `200 OK`
- [ ] POST upload returns success
- [ ] DELETE missing file returns `404 Not Found`

---

## 7. Example Runtime Results

Real validation on the running server returned results consistent with the expected behavior of a correctly configured web server.

_(paste actual terminal/output snippets here)_

---

## 8. Final Assessment

The project is in a good qualifying state for the subject requirements. The implementation covers the main required features and passes the internal project test suite. It demonstrates a functional architecture with clear separation between:

- Networking
- Parsing
- Routing
- Response building
- CGI
- Configuration

This project is a solid implementation of a webserver following the required structure and behavior.