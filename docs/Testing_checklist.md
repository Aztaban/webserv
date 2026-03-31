# Testing Checklist

## Purpose
This document is a practical checklist for validating the mandatory part of `webserv` during development.

Use it as a living file:
- add exact test commands,
- record bugs,
- mark what already passes,
- note unclear behavior to compare with NGINX.

The goal is not just to test success cases. The server must also stay stable under bad input, partial requests, disconnects, and stress.

---

## Table of content

1. [Tools](#1-tools)
2. [Build and startup](#2-build-and-startup)
3. [Event loop and connection handling](#3-event-loop-and-connection-handling)
4. [Request parsin](#4-request-parsing)
5. [Configuration parsing and application](#5-configuration-parsing-and-application)
6. [Get tests](#6-get-tests)
7. [Post tests](#7-post-tests)
8. [Delete tests](#8-delete-tests)
9. [Error page tests](#9-error-page-tests)
10. [Redirect tests](#10-redirect-tests)
11. [CGI tests](#11-cgi-tests)
12. [Browser checks](#12-browser-checks)
13. [Stress and resilience](#13-stress-and-resilience)
14. [Manual command section](#14-manual-command-section)
15. [Bug regression log](#15-bug-regression-log)
16. [Milestone checks](#16-milestone-checks)
17. [Final mandatory review](#17-final-mandatory-review)
18. [Alternative plain text status format](#18-alternative-plain-text-status-format)
19. [How to use this file](#19-how-to-use-this-file)

---

## 1. Tools
Use more than one testing method.

### Manual tools
- Web browser
- `curl`
- `nc` / `netcat`
- `telnet`

### Automated tools
- Python scripts for repeated requests
- simple stress scripts
- optional comparison setup with NGINX

---

## 2. Build and startup

### Build
- [ ] `make` works
- [ ] `make re` works
- [ ] project compiles with required flags
- [ ] Makefile does not relink unnecessarily
- [ ] no forbidden libraries are used

### Startup
- [ ] server starts with a valid config
- [ ] invalid config fails safely
- [ ] invalid port/bind case fails safely
- [ ] server opens all configured listening ports
- [ ] startup errors are understandable enough to debug

---

## 3. Event loop and connection handling

### Basic connection behavior
- [ ] one client can connect successfully
- [ ] multiple clients can connect successfully
- [ ] repeated connections do not crash the server
- [ ] client disconnect before full request does not crash the server
- [ ] incomplete request does not hang forever
- [ ] partial reads are handled correctly
- [ ] partial writes are handled correctly
- [ ] broken connection is cleaned up safely

### Non-blocking behavior
- [ ] socket I/O happens only when readiness is reported
- [ ] server stays responsive while several clients are connected
- [ ] slow client does not freeze other clients
- [ ] server remains usable during large or slow request transfer

---

## 4. Request parsing

### Request line
- [ ] valid GET request is parsed correctly
- [ ] valid POST request is parsed correctly
- [ ] valid DELETE request is parsed correctly
- [ ] malformed request line returns the correct error
- [ ] unsupported method returns the correct error
- [ ] invalid HTTP version is handled safely

### Headers
- [ ] normal headers are parsed correctly
- [ ] malformed headers return an error
- [ ] very large headers are handled safely
- [ ] duplicate or strange headers do not crash parsing

### Body handling
- [ ] request without body works correctly
- [ ] request with `Content-Length` body works correctly
- [ ] body can arrive in multiple chunks and still parse correctly
- [ ] oversized body returns correct error
- [ ] slow body upload does not break the server

---

## 5. Configuration parsing and application

### Config parser
- [ ] one server block parses correctly
- [ ] multiple server blocks parse correctly
- [ ] location blocks parse correctly
- [ ] invalid syntax is rejected safely
- [ ] missing required values are rejected safely

### Config behavior
- [ ] listening ports from config are applied
- [ ] error page config is applied
- [ ] max body size is applied
- [ ] allowed methods are applied
- [ ] redirect rules are applied
- [ ] root resolution is applied correctly
- [ ] index file behavior is applied correctly
- [ ] autoindex on/off is applied correctly
- [ ] upload path config is applied correctly
- [ ] CGI mapping by extension is applied correctly

---

## 6. GET tests

### Static files
- [ ] GET existing file returns `200`
- [ ] GET missing file returns `404`
- [ ] GET forbidden resource returns correct error
- [ ] HTML file content is returned correctly
- [ ] basic content type is correct for served files

### Directories
- [ ] directory with index file serves the index
- [ ] directory without index but with autoindex ON returns listing
- [ ] directory without index and autoindex OFF returns correct error

### Multiple ports / sites
- [ ] different ports can serve different content
- [ ] route resolution behaves correctly per configured server

---

## 7. POST tests

### Uploads
- [ ] POST upload to allowed route succeeds
- [ ] uploaded file is saved in configured location
- [ ] uploaded content is correct
- [ ] POST to route with uploads disabled returns correct error
- [ ] POST with oversized body returns correct error
- [ ] POST to invalid target returns correct error

### Request body behavior
- [ ] small upload works
- [ ] larger upload near limit works
- [ ] body sent in pieces still works

---

## 8. DELETE tests
- [ ] DELETE existing target succeeds when allowed
- [ ] DELETE missing target returns correct error
- [ ] DELETE on disallowed route returns correct error
- [ ] DELETE forbidden target returns correct error
- [ ] repeating DELETE after removal returns correct error

---

## 9. Error page tests
- [ ] default error page works when no custom page is configured
- [ ] custom error page works when configured
- [ ] correct HTTP status code matches the error page
- [ ] missing custom error page falls back safely

---

## 10. Redirect tests
- [ ] configured redirect returns correct 3xx code
- [ ] `Location` header is correct
- [ ] browser follows redirect as expected
- [ ] `curl -i` shows the expected redirect response

---

## 11. CGI tests

### Basic CGI behavior
- [ ] configured CGI script executes correctly
- [ ] CGI GET request works
- [ ] CGI POST request with body works
- [ ] CGI receives expected environment variables
- [ ] CGI runs from the correct working directory if needed

### CGI output handling
- [ ] CGI response headers are interpreted correctly
- [ ] CGI body is returned correctly
- [ ] CGI output without explicit content length is handled safely
- [ ] CGI failure does not crash the server

### CGI stability
- [ ] slow CGI does not break the whole server
- [ ] broken CGI script is handled safely
- [ ] CGI resources are cleaned up correctly

---

## 12. Browser checks
- [ ] static site loads in browser
- [ ] linked assets load correctly
- [ ] browser requests do not hang
- [ ] demo upload page works in browser if provided

---

## 13. Stress and resilience

### Stress cases
- [ ] many sequential GET requests do not crash server
- [ ] many simultaneous connections do not crash server
- [ ] repeated malformed requests do not crash server
- [ ] repeated connect/disconnect cycles do not break server
- [ ] large number of static file requests keeps server responsive
- [ ] CGI failures do not make the server unusable

### Good scenarios to automate
- [ ] many short GETs in a loop
- [ ] many clients connecting and closing immediately
- [ ] partial requests sent slowly
- [ ] upload close to maximum allowed body size
- [ ] repeated malformed headers

---

## 14. Manual command section
Replace these with your real commands as you build features.

```bash
curl -i http://localhost:8080/
curl -i http://localhost:8080/missing.html
curl -i -X POST --data-binary @test.txt http://localhost:8080/upload
curl -i -X DELETE http://localhost:8080/uploads/test.txt
printf 'GET / HTTP/1.1\r\nHost: localhost\r\n\r\n' | nc localhost 8080
```

---

## 15. Bug regression log
Record important bugs here so they do not come back.

### Template
```md
### Bug: short description
- Date:
- Found by:
- Trigger:
- Expected:
- Actual:
- Fix:
- Regression test command:
- Status:
```

---

## 16. Milestone checks

### Milestone A — server is alive
- [ ] listens on configured port
- [ ] accepts clients
- [ ] sends a valid HTTP response

### Milestone B — basic HTTP works
- [ ] request parsing works
- [ ] static GET works
- [ ] basic error responses work

### Milestone C — mandatory baseline works
- [ ] config parser works
- [ ] multiple ports work
- [ ] POST upload works
- [ ] DELETE works
- [ ] error pages work
- [ ] index / autoindex behavior works

### Milestone D — evaluation ready
- [ ] CGI works
- [ ] browser tests pass
- [ ] stress tests pass
- [ ] unclear behavior has been compared with NGINX
- [ ] demo configs and test files are prepared

---

## 17. Final mandatory review
Before evaluation, confirm all of these:
- [ ] non-blocking behavior is preserved
- [ ] one poll/equivalent model is used for required I/O
- [ ] GET is implemented
- [ ] POST is implemented
- [ ] DELETE is implemented
- [ ] static website serving works
- [ ] uploads work
- [ ] CGI works
- [ ] multiple ports work
- [ ] default error pages exist
- [ ] configured error pages work
- [ ] status codes are accurate
- [ ] server remains operational under stress
- [ ] each teammate can explain the architecture and request lifecycle

---

## 18. Alternative plain-text status format
If the canvas renderer still breaks checkbox display, use this style instead and update items manually:

```md
- [TODO] make works
- [TODO] server starts with a valid config
- [DONE] listens on configured port
- [BLOCKED] CGI POST request with body works
```

Recommended status labels:
- `[TODO]`
- `[IN PROGRESS]`
- `[DONE]`
- `[BLOCKED]`
- `[NOT TESTED]`

---

## 19. How to use this file
During the project, update this file in three ways:
1. mark what already works,
2. add exact commands/scripts you used,
3. record bugs and regression tests.

A checklist is only useful if it becomes part of the workflow.

