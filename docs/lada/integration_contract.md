# Request routing and partner integration contract

This document defines the stable boundary between configuration/routing, static request handling, redirects, and CGI.

## Ownership

- `ConfigParser` parses and validates configuration. It does not execute redirects or CGI.
- `Router` selects one virtual server and one location. It does not access the filesystem or execute CGI.
- `RequestHandler` merges the selected server and location into one `EffectiveRouteConfig`.
- The redirect partner owns redirect response construction and redirect-specific tests.
- The CGI partner owns CGI eligibility details, process lifecycle, environment creation, I/O, timeouts, output parsing, error mapping, and CGI-specific tests.

## Routing result

`Router::route(request, context)` returns a `RouteResult` containing:

- `server`: the selected virtual server on the current listener,
- `location`: the most specific matching location, or `NULL` for server-level fallback,
- `errorStatusCode`: a routing error such as `400` for a missing or malformed `Host` header.

No later component should repeat virtual-server or location selection.

## Virtual-server selection

The router:

1. retrieves `Host` case-insensitively,
2. trims optional whitespace,
3. removes a valid optional `:port`,
4. compares the hostname case-insensitively against every configured `server_name`,
5. falls back to the first server on the listener when the Host value is valid but unmatched,
6. returns `400` when Host is missing or malformed.

The first server is also the intentional fallback for errors that happen before a usable Host header can be selected.

## Location selection

Locations use longest-prefix matching on complete URI path segments.

- `/images` matches `/images/a.png`.
- `/images` does not match `/images2`.
- Location paths are normalized by the parser, so `/foo/` and `/foo` use one consistent representation.
- When no location matches, request handling uses server-level configuration.

## Effective configuration

`RequestHandler::buildEffectiveConfig(route)` is the only server/location merge point. It produces `EffectiveRouteConfig`, which includes:

- selected server and location pointers,
- effective root and index,
- effective method list and autoindex setting,
- upload enablement and upload destination,
- custom error-page mappings,
- body-size limit,
- redirect status and destination metadata,
- CGI enablement and extension/interpreter mappings.

Location values override server values only where location-level inheritance is supported. Upload, redirect, autoindex, and CGI behavior remain explicit location features.

## Request-handling order

After successful routing, `RequestHandler` must process a request in this order:

1. Build `EffectiveRouteConfig` once.
2. If `config.hasRedirect`, hand the request and effective config to the redirect implementation.
3. If the request is CGI-eligible under `config.cgiEnabled` and `config.cgiExtensions`, hand the parsed request and effective config to the CGI implementation.
4. Otherwise continue to static `GET`, upload `POST`, or `DELETE` handling.

Redirect and CGI handlers must not repeat Host selection, location matching, or configuration inheritance.

## CGI handoff data

The CGI module receives:

- the complete parsed `HttpRequest`, including method, target/path, headers, query string information, and body,
- `EffectiveRouteConfig`, including the selected server/location and CGI mappings.

The CGI partner decides interpreter selection, process execution, file descriptor ownership, timeout behavior, and conversion of CGI output into `HttpResponse`.

## Error and body-limit consistency

Once headers are available, `ClientConnection` uses the same Host-selection helper as `Router`. The selected server is reused for:

- `client_max_body_size`,
- early error responses,
- custom error-page selection.

A configured custom error file is attempted first. If it is missing or unreadable, the server preserves the original status code and returns the built-in generated error body.

## Stability rule

Changes to `RouteResult`, `EffectiveRouteConfig`, Host selection, or location selection must be coordinated with the redirect and CGI partners. New modules should consume these existing results rather than adding a second routing path.
