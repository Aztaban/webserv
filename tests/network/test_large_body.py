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


def test_oversized_content_length_returns_413(ctx: TestContext) -> None:
    """
    Send a body larger than the current temporary body limit.
    """
    request: str = (
        "POST / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: 1100000\r\n"
        "Connection: close\r\n"
        "\r\n"
    )

    response: str = ctx.send_raw_request(request, timeout=5)

    assert_http_response(response)
    assert_status(response, 413)


def test_oversized_body_upload_returns_413_or_closes_early(ctx: TestContext) -> None:
    body: str = "A" * 1100000

    response: str = ctx.post_allow_reset("/", body, timeout=5)

    assert_http_response(response)
    assert_status(response, 413)


def register_tests():
    return [
        ("large body does not crash server", test_large_body_does_not_crash_server),
        ("oversized content length returns 413", test_oversized_content_length_returns_413),
        ("oversized body upload returns 413 or closes early", test_oversized_body_upload_returns_413_or_closes_early)
    ]
