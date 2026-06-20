from tests.lib.assertions import assert_http_response, assert_status
from tests.lib.http_response import get_body
from tests.lib.test_context import TestContext


def test_cgi_get(ctx: TestContext) -> None:
    """
    Final expected behavior:
    Configured CGI script should execute and return generated output.
    """
    response: str = ctx.get("/cgi-bin/hello.py?name=webserv", timeout=5)

    assert_http_response(response)
    assert_status(response, 200)

    body: str = get_body(response)

    if "webserv" not in body:
        raise AssertionError("CGI response body should contain query result")


def register_tests():
    return [
        ("CGI GET executes script", test_cgi_get),
    ]
