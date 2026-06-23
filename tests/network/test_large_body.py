from tests.lib.assertions import assert_http_response, assert_status
from tests.lib.test_context import TestContext


def test_large_body_does_not_crash_server(ctx: TestContext) -> None:
    """
    Send a large but allowed request body.

    This verifies that body reading does not crash the server.
    """
    body: str = "A" * 10000

    response: str = ctx.post("/", body, timeout=5)

    assert_http_response(response)


def test_oversized_body_returns_413(ctx: TestContext) -> None:
    """
    Send a body larger than the current temporary body limit.

    With your current temporary ClientConnection limit, this should return 413.
    Later this limit should come from config.
    """
    body: str = "A" * 1100000

    response: str = ctx.post("/", body, timeout=5)

    assert_http_response(response)
    assert_status(response, 413)


def register_tests():
    return [
        ("large body does not crash server", test_large_body_does_not_crash_server),
        ("oversized body returns 413", test_oversized_body_returns_413)
    ]
