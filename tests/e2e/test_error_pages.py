from tests.lib.assertions import assert_http_response, assert_status
from tests.lib.test_context import TestContext


def test_default_404_error_page(ctx: TestContext) -> None:
    """Missing resource should return 404 error response."""
    response: str = ctx.get("/missing-error-page-test.html")

    assert_http_response(response)
    assert_status(response, 404)


def register_tests():
    return [
        ("default 404 error page", test_default_404_error_page),
    ]
