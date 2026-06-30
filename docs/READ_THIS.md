# Dev info

## Table of content

1. [Config (Lada)](#config-what-should-it-do)
2. [HTTP request parser (Diana)](#http-request-parser)
3. [Virtual server selection (Lada)](#virtual-server-selection)
4. [Location matching (Lada)](#location-matching)
5. [Resolving file path (Lada + Diana)](#resolving-file-path)
6. [GET handling (Diana)](#get-handling)
7. [Autoindex (Diana)](#autoindex)
8. [POST handling (Diana)](#post-handling)
9. [DELETE handling (Lada)](#delete-handling)
10. [Error pages (Diana)](#error-pages)
11. [Response builder (Diana)](#response-builder)
12. [Mime types (Diana)](#mime-types)
13. [Redirects (Lada)](#redirects)
14. [What is CGI? (Lada)](#what-is-cgi)
15. [Static file vs CGI (Diana + Lada)](#static-file-vs-cgi)
16. [How CGI works internally (Lada)](#how-cgi-works-internally)
17. [CGI environment variables (Lada)](#cgi-environment-variables)
18. [CGI output (Lada + Diana)](#cgi-output)
19. [CGI detection (Lada)](#cgi-detection)
20. [Important CGI danger (Lada)](#important-cgi-danger)
21. [Chunked transfer encoding (Diana)](#chunked-transfer-encoding)
22. [Keep-alive (Martin + Diana)](#keep-alive)
23. [How everything should work together](#how-everything-should-work-together)
24. [Recommended implementation order](#recommended-implementation-order)
25. [Mental model](#the-most-important-mental-model)


## Config: what should it do?

Example config:

```nginx
server {
    listen 8080;
    server_name localhost;
    root ./www;
    index index.html;
    client_max_body_size 1000000;

    error_page 404 /errors/404.html;

    location /images {
        root ./www;
        autoindex on;
        methods GET;
    }

    location /upload {
        root ./www;
        upload_store ./uploads;
        methods GET POST DELETE;
    }

    location /cgi-bin {
        root ./www;
        cgi_extension .py;
        cgi_path /usr/bin/python3;
        methods GET POST;
    }
}
```

Config parser should convert this text into structured objects:

```cpp
class ServerConfig {
    std::vector<int> ports;
    std::vector<std::string> serverNames;
    std::string root;
    std::vector<std::string> index;
    size_t clientMaxBodySize;
    std::map<int, std::string> errorPages;
    std::vector<LocationConfig> locations;
};

class LocationConfig {
    std::string path;              // "/upload"
    std::string root;
    std::vector<std::string> methods;
    bool autoindex;
    std::string uploadStore;
    std::map<std::string, std::string> cgi; // ".py" -> "/usr/bin/python3"
    std::string redirect;
};
```

>**The config parser’s job is not to handle requests. It only validates and stores rules.**

Important validations:

```
Is listen valid?
Are braces balanced?
Are duplicate/conflicting directives handled?
Are paths valid?
Is client_max_body_size numeric?
Are methods valid? GET / POST / DELETE
Are CGI interpreters configured correctly?
```

Good mental model:

```
Config = rules
Request = what client wants
Router = applies rules to request
```

---

## HTTP request parser

The parser receives raw bytes, not nice C++ objects.

Example raw request:

```http
POST /upload/file.txt HTTP/1.1
Host: localhost:8080
Content-Length: 11
Content-Type: text/plain

hello world
```

it should create something like:

```cpp
class HttpRequest {
public:
    std::string method;       // "POST"
    std::string target;       // "/upload/file.txt"
    std::string path;         // "/upload/file.txt"
    std::string query;        // after ?
    std::string version;      // "HTTP/1.1"
    std::map<std::string, std::string> headers;
    std::string body;
};
```

Usual parsing stages.

1. Read request line
2. Parse method, URI, version
3. Parse headers until empty line
4. Decide if body is expected
5. Read body using Content-Length or chunked encoding
6. Mark request complete

Example request line:

```http
GET /hello?name=bob HTTP/1.1
```

Should become:

```
method  = GET
target  = /hello?name=bob
path    = /hello
query   = name=bob
version = HTTP/1.1
```

Commone parser errors:

```
400 Bad Request        malformed request
405 Method Not Allowed method not allowed by config
413 Payload Too Large  body bigger than client_max_body_size
414 URI Too Long       optional, if you enforce it
501 Not Implemented    unsupported method
505 HTTP Version Not Supported
```

## Virtual server selection

This part matters when multiple `server` blocks listen on the same port.

Example 

```nginx
server {
    listen 8080;
    server_name site1.com;
    root ./site1;
}

server {
    listen 8080;
    server_name site2.com;
    root ./site2;
}
```

Browser sends:

```http
Host: site2.com:8080
```

Server should choose the second config.

Selection logic:

```
1. Use the local socket/port where the request arrived.
2. Find all server blocks listening on that port.
3. Use Host header to match server_name.
4. If no match, use the first/default server for that port.
```

So:

```
port 8080 + Host site2.com → server_name site2.com
port 8080 + unknown Host   → default server on 8080
```

---

## Location matching

After choosing the server, you choose the best `location`.

Example:

```nginx
location / {
    root ./www;
}

location /images {
    root ./assets;
}

location /images/private {
    root ./secret;
}
```

Request:

```http
GET /images/private/a.png HTTP/1.1
```

Best match should be:

```
/images/private
```

Usually, you want longest prefix match.

Pseudo:

```cpp
LocationConfig *matchLocation(std::string path, ServerConfig &server) {
    LocationConfig *best = NULL;

    for each location:
        if path starts with location.path:
            if best == NULL or location.path.length() > best.path.length():
                best = &location;

    return best;
}
```

---

## Resolving file path

This is one of the important parts.

Request:

```http
GET /images/cat.png
```

Config:

```nginx
location /images {
    root ./www;
}
```

You need map URL path to filesystem path.

Typical result:

```
URL path:         /images/cat.png
location path:   /images
root:            ./www
filesystem path: ./www/cat.png
```

But be careful: different projects implement root slightly differently. Some keep the full path:

```
./www/images/cat.png
```

The important thing is that your logic is consistent with your config design.

Also protect against path traversal:

```http
GET /../../etc/passwd
```

You should not allow escaping the root.

At minimum, normalize paths and reject dangerous cases.

---

## GET handling

`GET` means: give me a resource

Flow:

```
GET /something
        ↓
resolve config
        ↓
resolve path
        ↓
is it CGI?
        ↓ yes: execute CGI
        ↓ no:
            file exists?
            is directory?
            is regular file?
            permissions okay?
        ↓
build response
```

### File exists

Request:

```http
GET /index.html
```

Response:

```http
HTTP/1.1 200 OK
Content-Type: text/html
Content-Length: 125

<html>...</html>
```

### File does not exist

Response:

```http
HTTP/1.1 404 Not Found
Content-Type: text/html
Content-Length: ...
```

### Directory requested

Request:

```http
GET /docs/
```

If directory contains index.

```
/docs/index.html
```

serve it.

If no index and autoindex `on`:

```
generate HTML directory listing
```

If no index and autoindex off:

```
403 forbidden
```

---

## Autoindex

Autoindex means: when a user requests a directory and there is no index file, generate a page listing files in that directory.

Example response body:

```html
<html>
<body>
<h1>Index of /docs/</h1>
<a href="file1.txt">file1.txt</a><br>
<a href="notes/">notes/</a><br>
</body>
</html>
```

so:

```
GET /docs/
directory exists
no index.html
autoindex on
        ↓
server creates HTML dynamically
```

Autoindex is not CGI. It is generated directly by your server.

---

## POST handling

`POST` usually means: client sends data to the server.

POST is commonly used for:

```
file upload
form data
CGI input
```

Example:

```http
POST /upload HTTP/1.1
Content-Length: 11
Content-Type: text/plain

hello world
```

Server must:

```
read full body
check max body size
then decide:
    is this CGI? → pass body to CGI stdin
    is this upload route? → save body/file
    otherwise maybe 405 / 404 / 201 depending on your design
```

For simple upload, you can store body as a file:

```
uploads/generated_filename.txt
```

Response:

```http
HTTP/1.1 201 Created
Content-Length: ...

Upload successful
```

If body too large:

```http
HTTP/1.1 413 Payload Too Large
```

---

## DELETE handling

`DELETE` means: remove target resource.

```http
DELETE /upload/test.txt HTTP/1.1
Host: localhost:8080
```

Flow:

```
resolve path
method allowed?
file exists?
is it inside allowed root?
permission okay?
remove file
return status
```

Possible statuses:

```
204 No Content       deleted successfully, no response body
200 OK               deleted successfully with response body
403 Forbidden        no permission / trying to delete directory maybe
404 Not Found        target does not exist
405 Method Not Allowed
```

>Be careful with deleting directories. Unless your subject explicitly requires it, only delete regular files.

---

## Error pages

Config:

```nginx
error_page 404 /errors/404.html;
```

When a 404 happens, instead of returning a generic body, serve that file.

Flow:

```
handler decides status = 404
        ↓
response builder checks config error_page[404]
        ↓
if file exists: body = contents of custom file
        ↓
status line remains 404 Not Found
```

Important: serving the error page should not accidentally become a normal `200 OK`.

Correct:

```http
HTTP/1.1 404 Not Found
Content-Type: text/html
Content-Length: ...

<html>custom 404 page</html>
```

Not:

```http
HTTP/1.1 200 OK
```

---

## Response builder

A response is just text/binary bytes with a strict format:

```http
HTTP/1.1 200 OK
Header-Name: value
Another-Header: value

body
```

For example:

```http
HTTP/1.1 200 OK
Content-Type: text/html
Content-Length: 42
Connection: keep-alive

<html><body>Hello</body></html>
```

Your HttpResponse object can look like:

```cpp
class HttpResponse {
public:
    int statusCode;
    std::string reasonPhrase;
    std::map<std::string, std::string> headers;
    std::string body;

    std::string serialize() const;
};
```

Minimum important headers:

```
Content-Length
Content-Type
Connection
Location, for redirects
Allow, for 405
```

For binary files, do not treat everything as text conceptually. A PNG body is just bytes.

---

## MIME types

When serving files, browser wants to know what type it is.

Examples:

```
.html → text/html
.css  → text/css
.js   → application/javascript
.png  → image/png
.jpg  → image/jpeg
.txt  → text/plain
.json → application/json
```

Response:

```http
Content-Type: image/png
```

Without correct MIME types, browsers may display/download things weirdly.

---

## Redirects

Config may support something like:

```nginx
location /old {
    return 301 /new;
}
```

Request:

```http
GET /old
```

Response:

```http
HTTP/1.1 301 Moved Permanently
Location: /new
Content-Length: 0
```

Redirect means: “client, go request this other URL.”

Your server does not serve `/new` immediately. The browser makes another request.

---

## What is CGI?

CGI means Common Gateway Interface.

> CGI is a way for your web server to run an external program/script and use that program’s output as the HTTP response.

Without CGI, your server can only serve static files:

```
/index.html
/style.css
/image.png
```

With CGI, your server can generate dynamic content:

```
/time.py       → returns current time
/form.py       → processes POST body
/login.py      → checks submitted form
```

CGI is old, but it is very useful for learning because it teaches:

```
fork
execve
pipe
environment variables
stdin/stdout
process handling
HTTP integration
```

---

## Static file vs CGI

Static file:

```
GET /hello.html
        ↓
server reads ./www/hello.html
        ↓
server sends file content
```

CGI:

```
GET /script.py?name=bob
        ↓
server executes python3 script.py
        ↓
server gives CGI env variables
        ↓
script prints output to stdout
        ↓
server captures stdout
        ↓
server converts it into HTTP response
```

Example CGI script:

```python
#!/usr/bin/python3

print("Content-Type: text/html")
print()
print("<html><body>Hello from CGI</body></html>")
```

The CGI program prints headers and body. Your server reads that output.

---

## How CGI works internally

For a request like:

```http
GET /cgi-bin/hello.py?name=bob HTTP/1.1
Host: localhost:8080
```

Server does:

1. Detect that /cgi-bin/hello.py is CGI
2. Create pipes
3. fork()
4. Child:
    - set environment variables
    - connect stdin/stdout to pipes
    - execve("/usr/bin/python3", ["python3", "hello.py"], env)
5. Parent:
    - if POST: write request body to child's stdin
    - read child's stdout
    - wait / timeout
    - parse CGI output
    - build final HTTP response

Very simplified:

```cpp
int inPipe[2];   // parent writes request body, child reads stdin
int outPipe[2];  // child writes stdout, parent reads CGI output

pipe(inPipe);
pipe(outPipe);

pid_t pid = fork();

if (pid == 0) {
    dup2(inPipe[0], STDIN_FILENO);
    dup2(outPipe[1], STDOUT_FILENO);

    close unused fds;

    execve(interpreter.c_str(), argv, envp);
    exit(1);
} else {
    close unused fds;

    write(inPipe[1], request.body);
    close(inPipe[1]);

    read(outPipe[0], cgiOutput);
    waitpid(pid, ...);
}
```

---

## CGI environment variables

CGI programs receive request information through environment variables.

Common ones:

```
REQUEST_METHOD=GET
SCRIPT_NAME=/cgi-bin/hello.py
SCRIPT_FILENAME=./www/cgi-bin/hello.py
QUERY_STRING=name=bob
CONTENT_TYPE=application/x-www-form-urlencoded
CONTENT_LENGTH=11
SERVER_PROTOCOL=HTTP/1.1
SERVER_NAME=localhost
SERVER_PORT=8080
REMOTE_ADDR=127.0.0.1
PATH_INFO=...
```

For GET:

```http
GET /cgi-bin/hello.py?name=bob HTTP/1.1
```

CGI env:

```
REQUEST_METHOD=GET
QUERY_STRING=name=bob
CONTENT_LENGTH=
```

For POST:

```http
POST /cgi-bin/form.py HTTP/1.1
Content-Type: application/x-www-form-urlencoded
Content-Length: 13

name=bob&x=42
```

CGI env:

```
REQUEST_METHOD=POST
CONTENT_TYPE=application/x-www-form-urlencoded
CONTENT_LENGTH=13
```

And the body:

```
name=bob&x=42
```

goes to the CGI program’s `stdin`.

---

## CGI output

A CGI script usually outputs:

```http
Content-Type: text/html

<html>Hello</html>
```

Notice: CGI output normally starts with headers, but not the HTTP status line.

Your server should transform it into:

```http
HTTP/1.1 200 OK
Content-Type: text/html
Content-Length: 18

<html>Hello</html>
```

CGI may also output:

```http
Status: 404 Not Found
Content-Type: text/html

<h1>Not found</h1>
```

Then your server should produce:

```http
HTTP/1.1 404 Not Found
Content-Type: text/html
Content-Length: ...

<h1>Not found</h1>
```

So your CGI parser should understand:

```
Content-Type
Status
Location
empty line separating headers/body
```

---

## CGI detection

How do you know a request should run CGI?

Usually by config + file extension.

Example config:

```nginx
location /cgi-bin {
    cgi_extension .py;
    cgi_path /usr/bin/python3;
}
```

Request:

```http
GET /cgi-bin/test.py
```

Flow:

```
location matches /cgi-bin
file extension is .py
.py has configured interpreter
        ↓
execute CGI
```

Request:

```http
GET /cgi-bin/readme.txt
```

If `.txt` is not configured as CGI:

```
serve as static file or reject depending on config
```

## Important CGI danger

Never execute random files just because they exist.

Bad idea:

```
GET /uploads/user_file.py
```

If `/uploads` is not a CGI location, this should not execute.

Good rule:

```
Only execute CGI if:
1. matched location allows CGI
2. extension is configured
3. file exists
4. file is accessible
5. method is allowed
```

Also use a timeout. A CGI script can hang:

```python
while True:
    pass
```

> Your server should kill it after some limit.

---

## Chunked transfer encoding

Some clients send body like this:

```http
POST /upload HTTP/1.1
Transfer-Encoding: chunked

5
hello
6
 world
0
```

Body becomes:

```
hello world
```

Subject requires chunked input, your parser needs a chunked state machine.

Basic chunk parser:

```
read chunk size line in hex
read that many bytes
read CRLF
repeat
size 0 means end
```

Example:

```
5\r\n
hello\r\n
6\r\n
 world\r\n
0\r\n
\r\n
```

## Keep-alive

HTTP/1.1 uses persistent connections by default.

That means one TCP connection can handle multiple requests:

```
client sends request 1
server sends response 1
client sends request 2 on same socket
server sends response 2
```

Your client object should not assume:

```
one connection = one request
```

Instead:

```
socket buffer may contain:
- half a request
- exactly one request
- one request + part of another request
- multiple requests
```

After parsing one complete request, leave remaining bytes in the buffer for the next request.

Close connection if:

```
Connection: close
HTTP version requires close
fatal parse error
client disconnects
timeout
```

---

## How everything should work together

Example: `GET /`

```
client sends GET /
parser creates HttpRequest
server resolver chooses config by port + Host
location resolver chooses location /
path resolver maps / to ./www/
GET handler sees directory
index config says index.html
file ./www/index.html exists
response builder sends 200 with file
```

Example: `GET /folder/`

```
path is directory
try index files
if no index:
    autoindex on  → generate listing
    autoindex off → 403
```

Example: `POST /upload`

```
parser reads full body
checks Content-Length/client_max_body_size
location allows POST
if upload_store configured:
    save body/file
    return 201
else if CGI:
    pass body to CGI
else:
    return appropriate error
```

Example: `GET /cgi-bin/time.py`

```
location /cgi-bin matches
.py configured as CGI
server forks
child execve python3 time.py
server reads output
server builds HTTP response
```

---

## Recommended implementation order

```
1. Config parser
2. Request parser
3. Response builder
4. Static GET files
5. Location matching
6. Error pages
7. Directory index + autoindex
8. Method restrictions
9. POST body handling
10. Upload
11. DELETE
12. CGI GET
13. CGI POST
14. Keep-alive edge cases
15. Chunked body
16. Stress/error testing
```

Do not start with CGI. CGI is easier after your request parsing, path resolving, body reading, and response builder are stable.

---

## The most important mental model

Think of `webserv` as several separate machines:

```
Network layer:
    moves bytes in/out

Parser:
    bytes → HttpRequest

Config/router:
    request path/host/port → rules

Handler:
    rules + request → action

CGI/static/upload/delete:
    action implementation

Response builder:
    result → HTTP bytes
```

CGI is just one possible action:

```
Instead of reading a file and sending it,
run a program and send its output.
```

