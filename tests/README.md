# Webserv Test Suite

## Purpose

This test suite is used to validate the `webserv` project during development.

It is split into several test levels:

- `network/` — tests socket handling, poll loop behavior, disconnects, slow clients, many clients
- `e2e/` — tests full server behavior through real HTTP requests
- `unit/` — future C++ unit tests for parser, config, router, response builder
- `integration/` — future C++ tests for multiple internal components working together
- `manual/` — curl/netcat scripts for manual debugging

## Current Status

The runner currently supports:

- building the project with `make`
- starting `./webserv` with `tests/fixtures/test.conf`
- running Python tests from selected folders
- passing a shared `TestContext` to tests
- generating `tests/report.html`

## Usage

Run all supported tests:

```bash
python3 tests/runner.py
```

Run only network tests:

```bash
python3 tests/runner.py network
```

Run only e2e tests:

```bash
python3 tests/runner.py e2e
```

## Python Test File Format

Each Python test file should be named:

```text
test_*.py
```

Each file should expose:

```python
def register_tests():
    return [
        ("test name", test_function),
    ]
```

Each test function receives a shared context:

```python
def test_example(ctx):
    response = ctx.send_raw_request(
        "GET / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n"
    )

    assert response.startswith("HTTP/1.1")
```

## TODO: Multiport Support

The runner currently assumes one host/port:

```text
127.0.0.1:8000
```

To support multiport tests, add either:

1. extra fields to `TestContext`:

```python
ctx.primary_port
ctx.secondary_port
```

or

2. helper methods:

```python
ctx.send_raw_request_to_port(8001, request)
```

Planned tests:

- server opens port 8000
- server opens port 8001
- both ports respond
- different ports can later serve different content

## TODO: Multiple Config Support

The runner currently uses:

```text
tests/fixtures/test.conf
```

Later, support configs like:

```text
tests/fixtures/conf/basic.conf
tests/fixtures/conf/multiport.conf
tests/fixtures/conf/upload.conf
tests/fixtures/conf/cgi.conf
tests/fixtures/conf/small_body_limit.conf
```

Needed runner feature:

```bash
python3 tests/runner.py --config tests/fixtures/conf/multiport.conf
```

or per-test config metadata.

## TODO: C++ Unit Test Support

C++ unit tests are not run yet.

Planned structure:

```text
tests/unit/parser_tests.cpp
tests/unit/config_tests.cpp
tests/unit/router_tests.cpp
tests/integration/response_flow_tests.cpp
```

Possible compiled outputs:

```text
tests/bin/parser_tests
tests/bin/config_tests
tests/bin/router_tests
tests/bin/response_flow_tests
```

Runner should later execute these binaries and collect PASS/FAIL.

Possible future command:

```bash
python3 tests/runner.py unit
```

## TODO: Fixture Reset

Some tests will modify files, especially:

- upload tests
- DELETE tests
- CGI tests that write output

Before those tests exist, add fixture reset helpers:

```python
ctx.reset_uploads()
ctx.copy_fixture_file()
ctx.clean_generated_files()
```

This prevents one test from breaking another.

## TODO: More Test Groups

Future groups:

```text
network
e2e
unit
integration
stress
manual
```

## Generated Report

The runner writes:

```text
tests/report.html
```

This file is generated automatically from:

```text
tests/report_template.html
```

Do not edit `report.html` manually.

Recommended `.gitignore` entry:

```gitignore
tests/report.html
```

## Current Best Next Steps

1. Finish `runner.py`
2. Finish `tests/lib/*`
3. Add network tests:
   - basic connection
   - basic GET
   - connect/disconnect
   - many clients
   - slow client
   - large body
4. Add multiport support
5. Add placeholder files for future unit/e2e/integration tests