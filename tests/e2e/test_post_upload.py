from tests.lib.assertions import assert_http_response, assert_status
from tests.lib.test_context import TestContext


def test_post_upload_saves_file(ctx: TestContext) -> None:
    """
    Final expected behavior:
    POST /upload should save uploaded content into the configured upload folder.
    """
    ctx.reset_uploads()

    body: str = "hello from e2e upload test\n"

    response: str = ctx.post("/upload", body, timeout=5)

    assert_http_response(response)

    if not ctx.upload_contains(body):
        raise AssertionError("uploaded content was not found in upload directory")


def test_post_upload_content_exact(ctx: TestContext) -> None:
    """
    Final expected behavior:
    Uploaded file content should match the request body exactly.
    """
    ctx.reset_uploads()

    body: str = "exact upload content check\nline two\n"

    response: str = ctx.post("/upload", body, timeout=5)

    assert_http_response(response)

    uploaded_contents = []

    for path in ctx.upload_files():
        with open(path, "r", errors="ignore") as file:
            uploaded_contents.append(file.read())

    if body not in uploaded_contents:
        raise AssertionError("no uploaded file matched the request body exactly")


def test_post_to_upload_disabled_route_returns_error(ctx: TestContext) -> None:
    """
    Final expected behavior:
    POST to a route where upload is not enabled should return an error.

    Expected status may later be adjusted by the team.
    Common choices are 403 or 405.
    """
    ctx.reset_uploads()

    response: str = ctx.post("/", "body should not be uploaded here\n", timeout=5)

    assert_http_response(response)

    if "HTTP/1.1 403" not in response and "HTTP/1.1 405" not in response:
        raise AssertionError("POST to upload-disabled route should return 403 or 405")


def test_post_oversized_upload_returns_413(ctx: TestContext) -> None:
    """
    Final expected behavior:
    Upload larger than client_max_body_size should be rejected quickly.

    The project accepts either a clean 413 response or an early close/reset
    once the oversized upload is rejected.
    """
    ctx.reset_uploads()

    body: str = "A" * 1100000

    response: str = ctx.post_allow_reset("/upload", body, timeout=5)

    if response:
        assert_http_response(response)
        if "HTTP/1.1 413" not in response:
            raise AssertionError("oversized upload should be rejected with 413 or close early")
        return

    # A clean early connection close is also an acceptable rejection path.


def register_tests():
    return [
        ("POST upload saves file", test_post_upload_saves_file),
        ("POST upload content exact", test_post_upload_content_exact),
        ("POST to upload-disabled route returns error", test_post_to_upload_disabled_route_returns_error),
        ("POST oversized upload returns 413", test_post_oversized_upload_returns_413),
    ]
