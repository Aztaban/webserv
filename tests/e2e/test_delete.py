import os

from tests.lib.assertions import assert_http_response
from tests.lib.test_context import TestContext


def test_delete_existing_file(ctx: TestContext) -> None:
    """
    Final expected behavior:
    DELETE should remove an existing file when route allows it.
    """
    target_path: str = ctx.path("tests", "fixtures", "www", "uploads", "delete_me.txt")

    with open(target_path, "w") as file:
        file.write("delete me\n")

    response: str = ctx.send_raw_request(
        "DELETE /uploads/delete_me.txt HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Connection: close\r\n"
        "\r\n",
        timeout=3,
    )

    assert_http_response(response)

    if os.path.exists(target_path):
        raise AssertionError("DELETE did not remove target file")


def register_tests():
    return [
        ("DELETE existing file", test_delete_existing_file),
    ]
