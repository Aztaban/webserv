import os

from tests.lib.assertions import assert_http_response
from tests.lib.test_context import TestContext


def test_post_upload_saves_file(ctx: TestContext) -> None:
    """
    Final expected behavior:
    POST /upload should save uploaded content into the configured upload folder.
    """
    body: str = "hello from e2e upload test\n"

    response: str = ctx.post("/upload", body, timeout=5)

    assert_http_response(response)

    upload_dir: str = ctx.path("tests", "fixtures", "www", "uploads")

    found: bool = False

    if os.path.isdir(upload_dir):
        for filename in os.listdir(upload_dir):
            path: str = os.path.join(upload_dir, filename)

            if os.path.isfile(path):
                with open(path, "r", errors="ignore") as file:
                    if body in file.read():
                        found = True

    if not found:
        raise AssertionError("uploaded content was not found in upload directory")


def register_tests():
    return [
        ("POST upload saves file", test_post_upload_saves_file),
    ]
