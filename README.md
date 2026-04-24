# Webserv

## Description
Webserv is a custom HTTP server written in C++98.  
The goal of the project is to understand how web servers work internally by implementing core HTTP features from scratch.

The server:
- Handles multiple clients using non-blocking I/O
- Parses HTTP requests (GET, POST, DELETE)
- Serves static files
- Supports file uploads
- Executes CGI scripts
- Uses a configuration file to define behavior

This project follows the subject requirements strictly, including non-blocking behavior and use of a single poll()/equivalent mechanism for I/O. 

## How It Works

### 1. Start the server
Compile and run:

```bash
make
./webserv conf/default.conf
```

The program:
1. Parses the configuration file
2. Opens listening sockets
3. Enters an event loop
4. Waits for client connections

### 2. Send requests
Use browser or terminal:

```bash
curl -i http://localhost:8080/
curl -i http://localhost:8080/about.html
curl -i -X POST --data-binary @file.txt http://localhost:8080/upload
curl -i -X DELETE http://localhost:8080/uploads/file.txt
```

### 3. Request lifecycle
1. Client connects
2. Server accepts connection
3. Server reads request (non-blocking)
4. Request is parsed
5. Route is matched from config
6. Handler is selected (GET / POST / DELETE / CGI)
7. Response is generated
8. Response is sent
9. Connection is closed or reused

## Example Configuration

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

## Project Structure

```
webserv/
├── conf/
├── www/
├── cgi-bin/
├── include/
├── src/
├── docs/
└── tests/
```

## Features
- Non-blocking server (poll/select/epoll)
- Multiple client handling
- HTTP/1.1 basic support
- Static file serving
- File uploads (POST)
- File deletion (DELETE)
- CGI execution
- Multiple ports
- Configurable routes

## Resources
- RFC 7230 (HTTP/1.1)
- NGINX documentation
- 42 Webserv subject

## Notes
- NGINX is used only for comparison, not in implementation
- Server must never block or crash
- Testing should include curl, browser, and stress tests
