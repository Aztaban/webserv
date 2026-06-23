from tests.lib.assertions import assert_header_exists, assert_http_response
from tests.lib.test_context import TestContext


def test_configured_redirect(ctx: TestContext) -> None:
    """
    Final expected behavior:
    GET /old should return a redirect response with Location header.
    """
    response: str = ctx.get("/old")

    assert_http_response(response)
    assert_header_exists(response, "Location")

    if "HTTP/1.1 301" not in response and "HTTP/1.1 302" not in response:
        raise AssertionError("redirect should return 301 or 302")


def register_tests():
    return [
        ("configured redirect", test_configured_redirect),
    ]
