# Webserv config

Copy these folders into the root of webserv repository:

```bash
cp -r conf www cgi-bin tests /path/to/webserv/
chmod +x cgi-bin/hello.py tests/curl_commands.sh
```

Run one config at a time:

```bash
make
./webserv conf/default.conf
```

- `conf/default.conf`: main evaluation config; static files, upload, DELETE, redirect, autoindex, CGI, custom errors.
- `conf/multi_port.conf`: proves that two server blocks can listen on different ports.
- `conf/static_only.conf`: smaller config for early GET/static/autoindex testing.
- `conf/upload_delete.conf`: focused POST upload and DELETE testing.
- `conf/cgi.conf`: focused CGI testing.
- `conf/small_body_limit.conf`: focused 413 Payload Too Large testing.