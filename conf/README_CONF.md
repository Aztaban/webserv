# Webserv configuration fixtures

The files in this directory are runnable example configurations for the project.
Run one configuration at a time from the repository root:

```bash
make
./webserv conf/default.conf
```

The `run` target uses the default configuration as well:

```bash
make run
```

## Fixture overview

- `default.conf`: main integration fixture for static files, uploads, DELETE, redirects, autoindex, custom errors, and the CGI handoff configuration.
- `multi_port.conf`: two server blocks listening on different ports.
- `static_only.conf`: focused GET, static-file, and autoindex fixture.
- `upload_delete.conf`: focused upload and DELETE fixture.
- `cgi.conf`: CGI configuration fixture for the partner-owned CGI module.
- `small_body_limit.conf`: focused `413 Payload Too Large` fixture.

## Configuration grammar used by all fixtures

The examples intentionally use one strict, line-based grammar:

1. A server block starts with exactly `server {`.
2. A location block starts with `location <path> {`.
3. Opening braces stay on the block declaration line.
4. Closing braces appear alone on their own line.
5. Each directive occupies one line and ends with `;`.
6. Directive arguments are separated by whitespace.
7. Comments start with `#` and continue to the end of the line.
8. `listen` uses either `<port>` or `<IPv4-address>:<port>`, for example `127.0.0.1:8000`.
9. Paths are written relative to the repository root unless an absolute executable path is required.
10. Location paths begin with `/`.

Example:

```nginx
server {
    listen 127.0.0.1:8000;
    server_name example;

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
}
```


## Defaults, required directives, and inheritance

A configuration file must contain at least one `server` block. A server-level `root` directive is optional. Other server values use the
following defaults when their directives are omitted:

| Value | Default |
|---|---|
| listen host | `127.0.0.1` |
| listen port | `8000` |
| server names | none; the server is only a listener fallback until Host routing is enabled |
| root | empty by default; locations may provide their own root |
| index | `index.html` |
| client max body size | `1000000` bytes |
| error pages | none |
| locations | none |

A `server_name` directive accepts one or more whitespace-separated names. A
later `listen` directive changes only the listener host and port; it does not
reset names, root, index, error pages, body limits, or locations. An empty server root is preserved as an explicit “no inherited root” value.
The parser validates directive syntax and block structure, but it does not reject
a server solely because no root is configured. Request routing must use a
location root when there is no server-level root.

Location values follow this contract:

- `root` and `index` inherit from the selected server when omitted.
- Omitting `methods` allows all methods currently supported by the project:
  `GET`, `POST`, and `DELETE`. Writing `methods;` with no values is invalid.
- `autoindex` defaults to `off` and does not inherit.
- uploads default to disabled and do not inherit; `upload_path` is used only
  when upload handling is enabled for that location.
- redirects default to disabled and do not inherit.
- CGI extension/interpreter mappings stay stored on the location as metadata
  for the partner-owned CGI integration.
- location paths must start with `/`. Trailing slashes are removed except for
  `/`; query strings, fragments, backslashes, whitespace, repeated internal
  slashes, and `.` or `..` path segments are rejected.

## Supported fixture directives

Server context:

```text
listen
server_name
root
index
client_max_body_size
error_page
```

Location context:

```text
methods
root
index
autoindex
upload
upload_path
return
cgi_extension
```

`cgi_extension` is kept only as the configuration boundary for the CGI module. CGI process execution is not implemented by this task.
