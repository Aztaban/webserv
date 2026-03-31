# Request Lifecycle

## Purpose

This document describes the full path of a request through the server, from socket acceptance to response completion and cleanup.

---

## Table of content
1. [High-level lifecycle](#1-high-level-lifecycle)
2. [Detailed lifecycle](#2-detailed-lifecycle)
    - [Step 1 - listener readines](#step-1--listener-readiness)
    - [Step 2 - accept client](#step-2--accept-client)
    - [Step 3 - initialize client state](#step-3--initialize-client-state)
3. [Reading phase](#3-reading-phase)
    - [Step 4 - read available bytes](#step-4--read-available-bytes)
    - [Step 5 - parse request line and headers](#step-5--parse-request-line-and-headers)
    - [Step 6 - determine body requirements](#step-6--determine-body-requirements)
    - [Step 7 - read body incrementally](#step-7--read-body-incrementally)
4. [Processing phase](#4-processing-phase)
    - [Step 8 - route resolution](#step-8--route-resolution)
    - [Step 9 - resource/action resolution](#step-9--resourceaction-resolution)
5. [Writing phase](#5-writing-phase)
    - [Step 10 - prepare output buffer](#step-10--prepare-output-buffer)
    - [Step 11 - send response incrementally](#step-11--send-response-incrementally)
    - [Step 12 - finish response](#step-12--finish-response)
6. [Cleanup phase](#6-cleanup-phase)
7. [Suggested client states](#7-suggested-client-states)
8. [Request completion conditions](#8-request-completion-conditions)
9. [Error path principle](#9-error-path-principle)
10. [CGI Branch notes](#10-cgi-branch-notes)
11. [Lifecycle goal](#11-lifecycle-goal)

---

## 1. High-level lifecycle

1. A listening socket becomes readable.
2. The server accepts a new client connection.
3. The client file descriptor is registered in the event loop.
4. Incoming bytes are read incrementally when the socket is reported as readable.
5. Headers are parsed.
6. If a body is expected, the server continues reading until the full body is available.
7. The request is validated and routed.
8. A response is generated:

   * static file,
   * redirect,
   * autoindex,
   * upload result,
   * delete result,
   * CGI result,
   * or error response.
9. The response is sent incrementally when the socket is writable.
10. The connection is closed or reset safely.

---

## 2. Detailed lifecycle

### Step 1 — listener readiness

The event loop detects that a listening socket is readable.

### Step 2 — accept client

The server:

* accepts the client connection,
* sets the client socket to non-blocking mode,
* creates a `ClientConnection` object,
* registers the client fd in the poll/equivalent set.

### Step 3 — initialize client state

A new client connection starts with:

* empty input buffer,
* empty output buffer,
* fresh parser state,
* no selected route yet,
* state set to `READING_HEADERS`.

---

## 3. Reading phase

### Step 4 — read available bytes

When the client fd is readable:

* read available bytes into the input buffer,
* do not assume a complete request arrives in one read,
* handle disconnects safely,
* never perform read operations unless readiness was reported by poll/equivalent.

### Step 5 — parse request line and headers

The parser attempts to extract:

* method,
* target path,
* query string if present,
* HTTP version,
* headers.

If headers are incomplete:

* keep the connection in `READING_HEADERS`,
* wait for more readable events.

If headers are malformed:

* prepare an error response,
* move toward write phase.

### Step 6 — determine body requirements

After headers are parsed, determine whether a body is expected.
Possible cases:

* no body required,
* body with `Content-Length`,
* transfer mode needing special handling for CGI/body processing.

At this stage also validate:

* maximum body size,
* method/body compatibility,
* required upload settings when applicable.

### Step 7 — read body incrementally

If a body is expected:

* keep reading only when the socket is readable,
* append body data to the request buffer,
* track bytes received,
* stop when full body is collected.

If the body exceeds configured limits:

* stop normal processing,
* prepare the correct error response.

When the full request is available:

* move to `PROCESSING`.

---

## 4. Processing phase

### Step 8 — route resolution

The server determines:

* which server block applies,
* which location block matches best,
* what methods are allowed,
* whether redirect applies,
* which root/upload/index/autoindex/CGI settings are effective.

### Step 9 — resource/action resolution

Depending on route config and request method, the server chooses one of the following paths.

#### A. Static file GET

* resolve filesystem path,
* check existence and permissions,
* if target is directory, check index or autoindex rules,
* prepare file-based response or error.

#### B. POST upload

* validate upload authorization,
* resolve upload destination,
* write uploaded content,
* prepare success or failure response.

#### C. DELETE

* resolve target path,
* validate method authorization and resource existence,
* delete target if allowed,
* prepare success or failure response.

#### D. Redirect

* prepare redirect status and `Location` header.

#### E. CGI

* detect configured CGI by extension,
* prepare CGI environment,
* set up pipes,
* fork and exec the CGI,
* send request body to CGI if needed,
* collect CGI output,
* convert CGI output into HTTP response.

#### F. Error handling

At any point in resolution or processing, the server may switch to an error response such as:

* `400 Bad Request`
* `403 Forbidden`
* `404 Not Found`
* `405 Method Not Allowed`
* `413 Payload Too Large`
* `500 Internal Server Error`
* `502 Bad Gateway` for CGI-related failure if applicable to your design

---

## 5. Writing phase

### Step 10 — prepare output buffer

Once the final response is ready:

* serialize status line,
* serialize headers,
* append body or prepare file sending logic,
* move client state to `WRITING_RESPONSE`.

### Step 11 — send response incrementally

When the client fd is writable:

* send only the remaining bytes,
* update bytes-sent counters,
* keep unsent data in the output buffer,
* continue on later writable events if response is not fully sent.

Never assume the full response is written in one operation.

### Step 12 — finish response

After the full response is sent:

* either close the connection,
* or reset the connection state for another request if persistent handling is supported.

For an initial simpler implementation, closing after one response is acceptable as a development strategy.

---

## 6. Cleanup phase

Cleanup must happen safely on all exit paths.

### Connection cleanup includes

* removing fd from poll/equivalent tracking,
* closing socket,
* freeing parser/request/response resources,
* cleaning up CGI-related fds/process state if active.

### Cleanup triggers

* normal response completion,
* client disconnect,
* malformed request,
* timeout/stale connection policy,
* CGI failure,
* unrecoverable internal error.

---

## 7. Suggested client states

Use explicit states so the event loop remains understandable.

* `READING_HEADERS`
* `READING_BODY`
* `PROCESSING`
* `CGI_RUNNING`
* `WRITING_RESPONSE`
* `DONE`
* `ERROR`

Optional extra states if useful:

* `WRITING_TO_CGI`
* `READING_FROM_CGI`

---

## 8. Request completion conditions

A request is considered complete when:

* headers are complete,
* the body requirement is known,
* and the full body has been received if one is expected.

For CGI/body handling, the server must convert incoming body format into what the CGI expects before execution completes.

---

## 9. Error path principle

At every stage, any invalid condition should switch the connection from “normal processing” to “response preparation” rather than crashing or hanging.

Important rule:

* a request should never hang indefinitely.

That means the lifecycle must always have a clean failure path.

---

## 10. CGI branch notes

CGI deserves special attention because it adds another I/O path.

The CGI lifecycle is:

1. request is parsed and routed to CGI
2. CGI env is built
3. pipes are created
4. process is forked
5. child executes CGI with `execve`
6. parent writes body to CGI stdin if needed
7. parent reads CGI stdout incrementally
8. CGI output is parsed into headers/body
9. final HTTP response is built
10. CGI resources are cleaned up

This branch must not break the server’s non-blocking design.

---

## 11. Lifecycle goal

The request lifecycle must make it easy to answer these questions during evaluation:

* When do you know the request is complete?
* How do you avoid blocking?
* What happens if the client sends data slowly?
* What happens if the client disconnects mid-request?
* How do you switch from reading to writing?
* How does CGI fit into the same model?

If the team can explain this document clearly, the overall server design is usually in good shape.
