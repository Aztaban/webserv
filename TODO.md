# Webserv — Fix, Cleanup, Redirect, CGI Handoff, and Final Tester Task List

This checklist is based on the current project archive. The order is intentional: first remove temporary scaffolding and make configuration/routing reliable, then add redirect support, prepare a clean integration boundary for the partner-owned CGI module, and update the tester only after the server behavior is stable.

## Status legend

- **P0** — blocks correct configuration or routing
- **P1** — required feature or important integration fix
- **P2** — robustness, cleanup, or maintainability
- **DoD** — definition of done / acceptance criteria

---

## Phase 0 — Establish a clean baseline

### Build and repository cleanup

- [ ] **P0:** Rename `Makefile.txt` to `Makefile`.
- [ ] Verify all standard commands work from the repository root:
  - [ ] `make`
  - [ ] `make clean`
  - [ ] `make fclean`
  - [ ] `make re`
- [ ] Confirm the project still builds with `-Wall -Wextra -Werror -std=c++98`.
- [ ] Add or update `.gitignore` for generated files:
  - [ ] `build/`
  - [ ] `webserv`
  - [ ] test binaries under `tests/bin/`
  - [ ] `__pycache__/`
  - [ ] `*.pyc`
  - [ ] generated test reports and logs, if the team does not want them committed
- [ ] Remove committed generated artifacts that should not be source files.

### Remove temporary runtime scaffolding

- [ ] **P1:** Keep `main.cpp` using only `ConfigParser`; do not restore hardcoded server configurations.
- [ ] Remove temporary `std::cout` debug output from:
  - [ ] `src/main.cpp`
  - [ ] `src/config/ConfigParserBlocks.cpp`
  - [ ] `src/routing/Router.cpp`
- [ ] Use `Logger::debug()` for useful diagnostics that should remain available.
- [ ] Remove obsolete comments describing temporary behavior after the real implementation replaces it.
- [ ] Review and remove dead fields or parameters after confirming they are unused:
  - [ ] `RouteResult::root`
  - [ ] `HttpRequestHandler::_errorDirectory`
  - [ ] related constructor arguments and local variables

### Clean configuration fixtures

- [ ] Fix syntax mistakes in the files under `conf/`.
- [ ] Fix `127:0.0.1` to `127.0.0.1` where present.
- [ ] Add missing semicolons to directives.
- [ ] Make all example configs follow one documented grammar.

**DoD:** A fresh clone builds with plain `make`, starts using a real config file, and produces no temporary parser/router `std::cout` output.

---

## Phase 1 — Finish the configuration data model

### `ServerConfig`

- [ ] **P0:** Add storage for one or more `server_name` values.
- [ ] Add a getter such as `getServerNames()`.
- [ ] Add a method such as `addServerName()` or `setServerNames()`.
- [ ] Add safe setters for listener values, for example:
  - [ ] `setHost()`
  - [ ] `setPort()`
  - [ ] or one `setListen(host, port)` method
- [ ] Do not replace the whole `ServerConfig` object when parsing `listen`.
- [ ] Decide and document which values have defaults and which directives are required.

### `LocationConfig`

- [ ] Define the expected default behavior for an empty `methods` list.
  - Recommended: either set explicit defaults during parsing or reject an empty `methods;` directive.
- [ ] Define inheritance clearly:
  - [ ] location `root` falls back to server `root`
  - [ ] location `index` falls back to server `index`
  - [ ] methods, autoindex, upload, and redirect behavior are explicit
  - [ ] CGI mapping metadata remains available for the partner-owned integration
- [ ] Normalize location paths during parsing:
  - [ ] must start with `/`
  - [ ] remove a trailing `/` except for `/`
  - [ ] reject malformed paths

**DoD:** The config objects contain every value needed by routing and request handling, including `server_name`, without relying on temporary assumptions.

---

## Phase 2 — Make `ConfigParser` strict and order-independent

### Correct directive handling

- [ ] **P0:** Parse and store `server_name` at server level.
- [ ] Accept multiple names if that is part of the chosen grammar.
- [ ] Reject `server_name` inside a `location` block.
- [ ] **P0:** Fix `listen` so it updates only host and port and does not erase previously parsed values.
- [ ] Verify this works regardless of directive order:

```nginx
server {
    root ./www/site1;
    listen 127.0.0.1:8080;
}
```

- [ ] Require a semicolon after every directive.
- [ ] Reject unexpected text after `{` or `}`.
- [ ] Reject directives outside a `server` block.
- [ ] Reject nested `server` and nested `location` blocks.
- [ ] Reject a `location` block outside a `server` block.

### Numeric validation

- [ ] Replace unsafe `std::atoi` validation with a checked numeric parser.
- [ ] Detect:
  - [ ] empty values
  - [ ] non-numeric characters
  - [ ] negative values
  - [ ] overflow
  - [ ] values outside the destination type
- [ ] Validate `listen` port range: `1–65535`.
- [ ] Validate `client_max_body_size` against `std::size_t`.
- [ ] Validate `error_page` status codes against the accepted project range.
- [ ] Validate redirect codes; initially support the codes the team intends to implement, such as `301` and `302`.

### Directive argument validation

- [ ] Reject `methods;` with no methods.
- [ ] Reject duplicate methods in the same directive.
- [ ] Allow only supported methods: `GET`, `POST`, `DELETE`.
- [ ] Keep `cgi_extension` parsing compatible with the agreed CGI handoff contract.
- [ ] Store extension/interpreter mappings without executing or validating runtime availability in this scope.
- [ ] Validate `autoindex` as exactly `on` or `off`.
- [ ] Validate `upload` as exactly `on` or `off`.
- [ ] Reject an empty `upload_path`.
- [ ] Reject an empty redirect URL.
- [ ] Reject unknown directives at the correct block level.

### Structural and duplicate validation

- [ ] Reject duplicate location paths in one server.
- [ ] Decide how duplicate single-value directives are handled:
  - [ ] reject them, or
  - [ ] explicitly document that the last value wins
- [ ] Check consistency after a server block closes:
  - [ ] valid listener
  - [ ] valid server names
  - [ ] valid locations
  - [ ] upload settings are coherent
  - [ ] redirect settings are complete
  - [ ] CGI mapping metadata is internally consistent for handoff
- [ ] Include the line number and directive name in parser errors where possible.

**DoD:** Invalid configs fail immediately with a useful error, valid configs are independent of directive order, and no parsed value is silently discarded.

---

## Phase 3 — Finish server and location routing

### Select the virtual server

- [ ] **P0:** Implement `Router::selectServer()` using the request `Host` header.
- [ ] Retrieve headers case-insensitively or reuse a shared case-insensitive header helper.
- [ ] Normalize the `Host` value:
  - [ ] trim whitespace
  - [ ] remove an optional `:port`
  - [ ] compare host names case-insensitively
- [ ] Match the normalized host against every configured `server_name` on the current listener.
- [ ] If there is no matching name, use the first server on that listener as the default server.
- [ ] Define behavior for a malformed `Host` value and return `400` where appropriate.

### Select the location

- [ ] Keep longest-prefix matching.
- [ ] Match only complete URI path segments:
  - [ ] `/images` matches `/images/a.png`
  - [ ] `/images` does not match `/images2`
- [ ] Make normalized `/foo/` and `/foo` behavior consistent.
- [ ] Use `>` instead of `>=` after duplicate locations are rejected, so an equal-length later entry cannot silently replace the first.
- [ ] Decide the fallback when no location matches:
  - [ ] use server-level configuration, or
  - [ ] require/configure `/`
- [ ] Remove router debug prints.

### Routing API cleanup

- [ ] Add a small reusable helper for selecting a server from `Host` so early body-limit/error handling can use the same logic.
- [ ] Keep one source of truth for virtual-server selection; do not duplicate Host matching in multiple classes.
- [ ] Ensure `RouteResult` contains only data actually used by later stages.

**DoD:** Two servers on the same host/port return different content according to `Host`, and location matching always chooses the most specific valid location.

---

## Phase 4 — Remove first-server workarounds and complete integration

### Request body limit

- [ ] **P0:** Replace `ClientConnection::_getClientMaxBodySize()` using `serverCandidates[0]` unconditionally.
- [ ] Once request headers are available, select the server from `Host` before applying `client_max_body_size`.
- [ ] Use the listener's first server only when Host-based selection is impossible, such as an error before a valid Host header exists.
- [ ] Apply the same selected-server limit both while receiving the body and after parsing completes.

### Error pages

- [ ] Stop selecting the first server unconditionally in `ErrorResponseFactory` when a valid Host-selected server is known.
- [ ] Make configured `error_page` mappings actually take effect.
  - Current handler stores `_errorPages`, but `_error()` does not use them.
- [ ] Define fallback order:
  1. configured error page for the selected server
  2. generated built-in HTML error page
- [ ] Do not assume every error page is under `<root>/errors/<code>.html` if an explicit path was configured.
- [ ] Preserve correct status code even when the custom error file is missing.

### Effective route configuration

- [ ] Build one clear effective configuration from the selected server and location.
- [ ] Apply location overrides only where configured.
- [ ] Pass upload settings only when upload is enabled.
- [ ] Reject POST upload behavior on a location where upload is disabled.
- [ ] Confirm GET, POST, and DELETE all resolve paths relative to the selected location correctly.
- [ ] Check upload path joining so the directory and filename always have exactly one `/` between them.
- [ ] Review path traversal protection; do not rely only on searching for the substring `..`.

**DoD:** No runtime path uses `serverCandidates[0]` except an intentional documented fallback, and all handlers use the same selected server/location.

---

## Phase 5 — Implement redirects

- [ ] **P1:** Detect `route.location->hasRedirect` before static-file/upload/delete handling.
- [ ] Return the configured status code.
- [ ] Set the `Location` response header.
- [ ] Use the correct reason phrase for each supported redirect code.
- [ ] Decide whether the response body is empty or contains a small HTML message.
- [ ] Ensure `Content-Length` is correct through `HttpResponseBuilder`.
- [ ] Redirect should not attempt filesystem access.
- [ ] Validate redirect config during parsing rather than at request time.

**DoD:** A configured redirect returns the expected `301` or `302` and a correct `Location` header without accessing a static file.

---

## Phase 6 — Prepare the CGI integration handoff

CGI execution is owned by another partner. Do not implement process creation, pipes, `fork`, `execve`, timeouts, or CGI output parsing in this task list.

### Stable configuration and routing contract

- [ ] Keep parsed CGI extension/interpreter mappings accessible through `LocationConfig`.
- [ ] Ensure `RouteResult` exposes the selected server and selected location without requiring the CGI module to repeat Host or location matching.
- [ ] Ensure the normalized request path and query string remain available separately for the CGI module.
- [ ] Preserve request method, headers, and body in a form the CGI module can consume.
- [ ] Define one clear decision point where `RequestHandler` can hand a CGI-eligible request to the partner module before static-file handling.
- [ ] Do not hardcode CGI execution logic into `Router` or `ConfigParser`.
- [ ] Avoid changing public config/routing APIs after the handoff contract is agreed.
- [ ] Document ownership boundaries:
  - [ ] this scope owns config parsing, virtual-server selection, location selection, redirects, and core integration
  - [ ] the CGI partner owns detection details, process lifecycle, environment creation, I/O, output parsing, errors, and CGI tests

### Integration safety

- [ ] Keep non-CGI requests fully functional when the CGI module is absent or disabled.
- [ ] Ensure a CGI-enabled location does not accidentally fall through to static-file execution once the partner hook is connected.
- [ ] Agree on the return/result type for CGI handoff, including pending, completed, and failed states.
- [ ] Agree on who owns file descriptors, child processes, timeouts, and response construction before merging CGI code.

**DoD:** The CGI partner can plug the module into one documented handoff point using the selected server/location and parsed request data, without changing parser or router behavior.

---

## Phase 7 — General behavior and cleanup pass

- [ ] Review all response reason phrases and status codes.
- [ ] Verify configured method restrictions return the chosen correct status, normally `405`.
- [ ] Add an `Allow` header for `405` if required by the project expectations.
- [ ] Verify upload-disabled POST cannot save a file.
- [ ] Verify DELETE cannot remove directories or escape the configured root.
- [ ] Verify autoindex links work for nested directories.
- [ ] Verify MIME types for HTML, CSS, JavaScript, images, and plain text.
- [ ] Verify query strings do not become part of filesystem paths.
- [ ] Check partial socket reads/writes and slow clients.
- [ ] Check all return paths for FD/resource leaks.
- [ ] Run `valgrind` or the team's chosen leak checker on representative requests.
- [ ] Remove stale TODOs that are complete and keep only actionable TODOs.
- [ ] Update architecture/config documentation to match actual behavior.

**DoD:** Source code has no temporary branches, misleading comments, uncontrolled debug output, or known resource leaks.

---

## Phase 8 — Update the tester last

Do this only after the implementation and cleanup phases above are stable, so tests describe the final intended behavior rather than temporary behavior. CGI execution tests remain owned by the CGI partner and are not part of this checklist.

### Fix the runner

- [ ] Ensure `tests/runner.py` builds with plain `make`.
- [ ] Add `router_tests.cpp` to `CPP_UNIT_TESTS` with all required source files.
- [ ] Add real integration-test binaries to the runner.
- [ ] Decide whether integration tests are a separate group:
  - [ ] `python3 tests/runner.py integration`
  - [ ] include integration tests in `all`
- [ ] Make server startup failures print the relevant server log.
- [ ] Ensure the test server is always stopped, including compile/test exceptions.
- [ ] Remove stale generated binaries before compiling tests.
- [ ] Keep test results deterministic and isolated.

### Fix broken existing tests and fixtures

- [ ] Fix the malformed call in `tests/e2e/test_post_upload.py`:

```python
ctx.post("/upload, body, timout=5")
```

- [ ] Replace it with the intended call using separate path/body arguments and the correct `timeout` spelling.
- [ ] Add missing fixture files required by browser asset tests:
  - [ ] CSS
  - [ ] JavaScript
  - [ ] image
- [ ] Ensure the test upload directory is created and reset reliably.
- [ ] Add redirect locations to `tests/fixtures/test.conf` when redirects are implemented.
- [ ] Add two virtual servers on the same port with different `server_name` values.

### Expand config parser unit tests

- [ ] Valid server with all supported server directives.
- [ ] Valid location with all supported location directives.
- [ ] Multiple servers and multiple locations.
- [ ] `server_name` parsing.
- [ ] `listen` after `root` does not reset the root.
- [ ] Missing semicolon.
- [ ] Invalid port: zero, negative, non-numeric, and over `65535`.
- [ ] Overflowing body size.
- [ ] Unknown directive.
- [ ] Directive at the wrong block level.
- [ ] Empty `methods`.
- [ ] Invalid method.
- [ ] Invalid redirect code.
- [ ] Duplicate location.
- [ ] Nested blocks.
- [ ] Unclosed blocks.
- [ ] Trailing garbage after braces.

### Replace router placeholder tests

- [ ] Select matching server by exact `Host`.
- [ ] Host comparison is case-insensitive.
- [ ] Host with `:port` matches the server name.
- [ ] Unknown Host falls back to the first server.
- [ ] Exact location match.
- [ ] Longest-prefix location match.
- [ ] Segment-boundary case: `/images` does not match `/images2`.
- [ ] Normalized trailing-slash location.
- [ ] Root fallback.
- [ ] Empty candidate list returns no route safely.

### Add integration tests

- [ ] Config parser → virtual server selection → location selection.
- [ ] Selected location values reach `HttpRequestHandler`.
- [ ] Per-server body-size limit follows Host selection.
- [ ] Per-server custom error page follows Host selection.
- [ ] Redirect config produces a redirect response.

### Expand end-to-end coverage

- [ ] GET static file and index.
- [ ] CSS, JavaScript, and image assets.
- [ ] Autoindex on/off.
- [ ] POST upload success.
- [ ] POST upload disabled.
- [ ] Oversized body.
- [ ] DELETE existing/missing/forbidden target.
- [ ] Redirect status and `Location` header.
- [ ] Virtual hosts on one listener.
- [ ] Multiple ports.
- [ ] Custom error pages.
- [ ] Slow client and many simultaneous clients.

### Final test commands

- [ ] `make re`
- [ ] `python3 tests/runner.py unit`
- [ ] `python3 tests/runner.py integration`
- [ ] `python3 tests/runner.py network`
- [ ] `python3 tests/runner.py e2e`
- [ ] `python3 tests/runner.py`
- [ ] Confirm every test passes from a clean checkout.
- [ ] Confirm repeated test runs do not depend on files left by a previous run.

**DoD:** The final tester covers config parsing, virtual hosts, location routing, redirects, errors, uploads, DELETE, autoindex, and concurrent network behavior, with no placeholder or knowingly broken tests. CGI execution coverage is maintained by the CGI partner.

---

## Suggested implementation order / commit split

1. [ ] `chore: restore Makefile and remove temporary debug output`
2. [ ] `config: add server_name and safe listen setters`
3. [ ] `config: make parser strict and order-independent`
4. [ ] `router: implement Host-based virtual server selection`
5. [ ] `core: remove first-server fallbacks and apply selected config`
6. [ ] `http: implement configured redirects`
7. [ ] `core: expose stable CGI handoff boundary`
8. [ ] `cleanup: error pages, paths, resources, and documentation`
9. [ ] `tests: replace placeholders and update final runner`

---

## Final project completion criteria

- [ ] Configuration is loaded exclusively from the selected config file.
- [ ] Invalid configuration is rejected before listeners start.
- [ ] `Host` selects the correct virtual server.
- [ ] Longest valid location match is selected.
- [ ] GET, POST, DELETE, upload, autoindex, redirects, and custom errors use the selected config.
- [ ] Parser, router, and request handling expose a stable handoff point for the partner-owned CGI module.
- [ ] No first-server workaround remains except documented fallback behavior.
- [ ] No temporary debug output or generated files remain in source control.
- [ ] Clean build and complete tester pass from a fresh checkout.
