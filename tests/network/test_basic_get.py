from tests.lib.test_context import TestContext
from tests.lib.assertions import (
    assert_has_header_body_separator,
    assert_header_exists,
    assert_http_response,
    assert_status,
)


def test_get_root_returns_200(ctx):
    """Verify that GET / returns HTTP status 200."""
    response: str = ctx.get("/")

    assert_http_response(response)
    assert_status(response, 200)


def test_response_has_content_length(ctx: TestContext) -> None:
    """Verify that the response includes a Content-Length header."""
    response: str = ctx.get("/")

    assert_http_response(response)
    assert_header_exists(response, "Content-Length")


def test_response_has_header_body_separator(ctx: TestContext) -> None:
    """Verify that headers and body are separated by CRLF CRLF."""
    response: str = ctx.get("/")

    assert_http_response(response)
    assert_has_header_body_separator(response)


def register_tests():
    return [
        ("GET / returns 200", test_get_root_returns_200),
        ("response has Content-Length", test_response_has_content_length),
        ("response has header/body separator", test_response_has_header_body_separator)
    ]
