# Architecture

## Purpose

This document describes the internal structure of the `webserv` project: the main subsystems, their responsibilities, and how they interact.

---

## Table of content
1. [System overview](#1-system-overview)
2. [Main subsystems](#2-main-subsystems)
3. [Suggested core classes and ownership](#3-suggested-core-classes-and-ownership)
4. [Interaction flow between subsystems](#4-interaction-flow-between-subsystems)
5. [Key design decision](#5-key-design-decisions)
6. [Integration boundaries](#6-integration-boundaries)
7. [Architecture goals](#7-architecture-goals)

---

## 1. System overview

`webserv` is a non-blocking HTTP server written in **C++98**. Its job is to:

* listen on one or more configured ports,
* accept and manage multiple client connections,
* parse HTTP requests,
* apply server and route configuration,
* generate correct HTTP responses,
* serve static files,
* handle uploads and DELETE,
* execute CGI when configured,
* remain stable under invalid input and stress.

The project should be organized around a small number of clear subsystems instead of mixing networking, parsing, filesystem access, and CGI logic together.

---

## 2. Main subsystems

### 2.1 Config subsystem

Responsible for reading and validating the configuration file.

#### Responsibilities

* read config file content
* tokenize and parse directives
* build internal server/location configuration objects
* validate required values and syntax
* expose final config to the runtime server

#### Main elements

* `ConfigParser`
* `ServerConfig`
* `LocationConfig`
* config validation helpers

---

### 2.2 Network / event loop subsystem

Responsible for all socket lifecycle management and readiness-driven I/O.

#### Responsibilities

* create listening sockets
* set sockets to non-blocking mode
* register all relevant file descriptors in one poll/equivalent loop
* accept new client connections
* detect read/write readiness
* forward raw input/output work to the proper connection handlers
* safely clean up disconnected or finished clients

#### Main elements

* `WebServer`
* `EventLoop`
* `Listener`
* `ClientConnection`

This subsystem should not contain HTTP parsing logic beyond dispatching incoming bytes to the request parser.

---

### 2.3 HTTP parsing subsystem

Responsible for transforming raw bytes into structured HTTP request data.

#### Responsibilities

* parse request line
* parse headers
* detect malformed requests
* collect request body incrementally
* enforce limits such as maximum body size
* detect whether the request is complete

#### Main elements

* `HttpRequest`
* `HttpRequestParser`

The parser should be testable independently from the socket layer.

---

### 2.4 Routing / request resolution subsystem

Responsible for deciding how a request should be handled after parsing.

#### Responsibilities

* select the matching server configuration
* match the request path against location rules
* resolve effective configuration for the request
* check allowed methods
* determine whether request becomes:

  * static file response
  * index file response
  * autoindex response
  * redirect
  * upload handling
  * DELETE handling
  * CGI execution
  * error response

#### Main elements

* `Router`
* `ResourceResolver`

---

### 2.5 Response subsystem

Responsible for building the final HTTP response.

#### Responsibilities

* generate status line
* generate headers
* attach response body or file target
* generate default/custom error pages
* create redirect responses
* prepare response for incremental send

#### Main elements

* `HttpResponse`
* `HttpResponseBuilder`
* status code helpers
* MIME type helpers

---

### 2.6 CGI subsystem

Responsible for running configured CGI scripts and translating their output into HTTP responses.

#### Responsibilities

* detect CGI by extension and route config
* prepare CGI environment variables
* set up pipes
* fork and exec CGI process
* provide request body to CGI when needed
* read CGI output safely
* parse CGI output headers/body
* return CGI result as an HTTP response

#### Main elements

* `CgiHandler`
* environment builder
* CGI output parser

CGI should remain isolated from the normal static file path as much as possible.

---

### 2.7 Utility subsystem

Common helpers used by the rest of the project.

#### Responsibilities

* path joining/normalization
* MIME type lookup
* string utilities
* file helpers
* debug/log helpers
* status text helpers

#### Main elements

* `PathUtils`
* `MimeTypes`
* `StringUtils`
* `Logger`

---

## 3. Suggested core classes and ownership

### Runtime / core

* `WebServer`

  * initializes config
  * creates listeners
  * starts event loop
  * coordinates shutdown/cleanup

### Network

* `EventLoop`

  * wrapper around `poll()` or chosen equivalent
  * stores monitored file descriptors
  * dispatches read/write events
* `Listener`

  * holds one listening socket
  * accepts incoming client fds
* `ClientConnection`

  * stores per-client state
  * owns buffers, parser state, and response progress

### Config

* `ConfigParser`
* `ServerConfig`
* `LocationConfig`

### HTTP

* `HttpRequest`
* `HttpRequestParser`
* `HttpResponse`
* `HttpResponseBuilder`
* `Router`
* `ResourceResolver`

### CGI

* `CgiHandler`

### Utils

* `MimeTypes`
* `PathUtils`
* `Logger`

---

## 4. Interaction flow between subsystems

1. `WebServer` loads and validates configuration.
2. `WebServer` creates one or more `Listener` objects.
3. `EventLoop` monitors listening sockets and active client connections.
4. When a client sends data, `ClientConnection` receives bytes.
5. `HttpRequestParser` parses the request incrementally.
6. Once the request is complete, `Router` selects the effective route behavior.
7. `ResourceResolver` determines the actual target resource or action.
8. If CGI is required, `CgiHandler` executes it and collects output.
9. `HttpResponseBuilder` creates the final response.
10. `ClientConnection` sends the response incrementally through the event loop.
11. The connection is either closed or reset for another request, depending on final server behavior.

---

## 5. Key design decisions

### Separation of concerns

Networking, parsing, routing, filesystem access, and CGI should stay separate. This makes debugging and testing far easier.

### Explicit client states

Each connection should have a clear state rather than scattered booleans.
Examples:

* `READING_HEADERS`
* `READING_BODY`
* `PROCESSING`
* `CGI_RUNNING`
* `WRITING_RESPONSE`
* `DONE`
* `ERROR`

### Incremental parsing

Do not assume the full request arrives in one `recv()`. Parsing must handle partial input.

### Isolated CGI path

CGI should not be mixed into the static file serving path more than necessary.

### Minimal config grammar

Support only what the subject requires. Avoid building a full nginx clone.

---

## 6. Integration boundaries

To let multiple people work in parallel, these interfaces should be agreed early:

* what `HttpRequestParser` receives and returns
* what fields `HttpRequest` must contain
* how `Router` receives request + config and returns effective route info
* how `HttpResponseBuilder` receives route result and builds response
* how `ClientConnection` stores parser and response state
* how CGI execution reports success/failure/output

---

## 7. Architecture goals

The architecture should optimize for:

* correctness,
* non-blocking behavior,
* easy debugging,
* independent testing of subsystems,
* safe teamwork and integration,
* explainability during evaluation.

If a design choice makes the code harder to explain, harder to test, or harder to keep non-blocking, it is probably the wrong choice.
