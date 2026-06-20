from tests.lib.assertions import assert_http_response, assert_status
from tests.lib.http_response import get_body
from tests.lib.test_context import TestContext


def test_get_existing_index(ctx: TestContext) -> None:
    """
    Final expected behavior:
    GET /index.html should serve the test index file.
    """
    response: str = ctx.get("/index.html")

    assert_http_response(response)
    assert_status(response, 200)

    body: str = get_body(response)

    if "Webserv Test Index" not in body:
        raise AssertionError("index.html body was not returned")


def test_get_missing_file_returns_404(ctx: TestContext) -> None:
    """GET missing file should return 404."""
    response: str = ctx.get("/missing.html")

    assert_http_response(response)
    assert_status(response, 404)


def register_tests():
    return [
        ("GET existing index file", test_get_existing_index),
        ("GET missing file returns 404", test_get_missing_file_returns_404),
    ]
