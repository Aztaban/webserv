import os

from tests.lib.assertions import assert_http_response, assert_status
from tests.lib.test_context import TestContext


def test_delete_existing_file(ctx: TestContext) -> None:
    """
    Final expected behavior:
    DELETE should remove an existing file when route allows it.
    """
    ctx.reset_uploads()

    target_path: str = ctx.write_fixture_file(
        "tests/fixtures/www/uploads/delete_me.txt",
        "delete me\n",
    )

    response: str = ctx.send_raw_request(
        "DELETE /uploads/delete_me.txt HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Connection: close\r\n"
        "\r\n",
        timeout=3,
    )

    assert_http_response(response)

    assert_status(response, 204)
    headers, body = response.split("\r\n\r\n", 1)
    if body:
        raise AssertionError("204 DELETE response must have no body")
    names = [line.split(":", 1)[0].lower() for line in headers.split("\r\n")[1:]]
    if 'content-length' in names or 'transfer-encoding' in names:
        raise AssertionError("204 DELETE response must omit framing headers")

    if os.path.exists(target_path):
        raise AssertionError("DELETE did not remove target file")


def test_delete_missing_file_returns_404(ctx: TestContext) -> None:
    """
    Final expected behavior:
    DELETE on a missing file should return 404.
    """
    ctx.reset_uploads()

    response: str = ctx.send_raw_request(
        "DELETE /uploads/missing_delete_target.txt HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Connection: close\r\n"
        "\r\n",
        timeout=3,
    )

    assert_http_response(response)
    assert_status(response, 404)


def register_tests():
    return [
        ("DELETE existing file", test_delete_existing_file),
        ("DELETE missing file returns 404", test_delete_missing_file_returns_404),
    ]
