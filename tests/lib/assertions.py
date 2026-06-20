def assert_http_response(response: str) -> None:
    """Check that the server returned something shaped like an HTTP response."""
    if not response.startswith("HTTP/1.1"):
        raise AssertionError("response should start with HTTP/1.1")


def assert_status(response: str, status_code: int) -> None:
    """Check that the response status line contains the expected status code."""
    status_line: str = response.split("\r\n", 1)[0]
    expected: str = "HTTP/1.1 {}".format(status_code)

    if not status_line.startswith(expected):
        raise AssertionError(
            "expected status {}, got status line: {}".format(
                status_code,
                status_line,
            )
        )


def assert_header_exists(response:str, header_name: str) -> None:
    """Check that a response contains a given HTTP header."""
    header_section: str = response.split("\r\n\r\n", 1)[0]
    expected: str = header_name.lower() + ":"

    if expected not in header_section.lower():
        raise AssertionError("expected header '{}'".format(header_name))


def assert_has_header_body_separator(response: str) -> None:
    """Check that HTTP headers and body are separated by CRLF CRLF."""
    if "\r\n\r\n" not in response:
        raise AssertionError("response should contain CRLF header/body separator")


def assert_body_contains(response: str, expected_text: str) -> None:
    """Check that expected text appears somewhere in the response."""
    if expected_text not in response:
        raise AssertionError("response body should contain '{}'".format(expected_text))
