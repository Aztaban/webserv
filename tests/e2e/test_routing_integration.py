import os

from tests.lib.assertions import (
    assert_body_contains,
    assert_http_response,
    assert_status,
)
from tests.lib.test_context import TestContext


def _request(
    ctx: TestContext,
    method: str,
    path: str,
    host: str,
    body: str = "",
    timeout: float = 5.0,
) -> str:
    headers = (
        "{} {} HTTP/1.1\r\n"
        "Host: {}\r\n"
        "Connection: close\r\n"
    ).format(method, path, host)

    if body:
        headers += "Content-Length: {}\r\n".format(len(body))

    return ctx.send_raw_request(headers + "\r\n" + body, timeout=timeout)


def test_virtual_servers_share_one_listener(ctx: TestContext) -> None:
    primary = _request(ctx, "GET", "/", "PrImArY.TeSt:8000")
    alternate = _request(ctx, "GET", "/", "ALTERNATE.TEST:8000")

    assert_http_response(primary)
    assert_status(primary, 200)
    assert_body_contains(primary, "Webserv Test Index")

    assert_http_response(alternate)
    assert_status(alternate, 200)
    assert_body_contains(alternate, "ALTERNATE VIRTUAL SERVER")


def test_unknown_host_uses_default_server(ctx: TestContext) -> None:
    response = _request(ctx, "GET", "/", "unknown.test")

    assert_http_response(response)
    assert_status(response, 200)
    assert_body_contains(response, "Webserv Test Index")


def test_malformed_host_returns_400(ctx: TestContext) -> None:
    response = _request(ctx, "GET", "/", "alternate.test:not-a-port")

    assert_http_response(response)
    assert_status(response, 400)


def test_longest_location_and_segment_boundary(ctx: TestContext) -> None:
    shallow = _request(ctx, "GET", "/route/marker.txt", "primary.test")
    deep = _request(ctx, "GET", "/route/deep/marker.txt", "primary.test")
    boundary = _request(ctx, "GET", "/route2/marker.txt", "primary.test")

    assert_status(shallow, 200)
    assert_body_contains(shallow, "SHALLOW ROUTE")

    assert_status(deep, 200)
    assert_body_contains(deep, "DEEP ROUTE")

    assert_status(boundary, 404)


def test_body_limit_follows_selected_server(ctx: TestContext) -> None:
    body = "A" * 64
    primary_path = ctx.path("tests", "fixtures", "www", "uploads", "host-limit.txt")

    if os.path.exists(primary_path):
        os.remove(primary_path)

    primary = _request(
        ctx,
        "POST",
        "/upload/host-limit.txt",
        "primary.test",
        body,
    )
    alternate = _request(
        ctx,
        "POST",
        "/upload/host-limit.txt",
        "alternate.test",
        body,
    )

    assert_status(primary, 201)
    assert_status(alternate, 413)

    if os.path.exists(primary_path):
        os.remove(primary_path)


def test_body_limit_can_be_overridden_per_location(ctx: TestContext) -> None:
    accepted_path = ctx.path("tests", "fixtures", "www", "uploads", "location-ok.txt")
    chunked_path = ctx.path("tests", "fixtures", "www", "uploads", "location-chunked.txt")

    for path in (accepted_path, chunked_path):
        if os.path.exists(path):
            os.remove(path)

    accepted = _request(
        ctx,
        "POST",
        "/limited/location-ok.txt",
        "primary.test",
        "A" * 16,
    )
    rejected = _request(
        ctx,
        "POST",
        "/limited/location-too-big.txt",
        "primary.test",
        "B" * 17,
    )
    chunked_rejected = ctx.send_raw_request(
        "POST /limited/location-chunked.txt HTTP/1.1\r\n"
        "Host: primary.test\r\n"
        "Transfer-Encoding: chunked\r\n"
        "Connection: close\r\n"
        "\r\n"
        "11\r\n"
        + ("C" * 17)
        + "\r\n0\r\n\r\n"
    )

    assert_status(accepted, 201)
    assert_status(rejected, 413)
    assert_status(chunked_rejected, 413)

    for path in (accepted_path, chunked_path):
        if os.path.exists(path):
            os.remove(path)

def test_custom_error_page_follows_selected_server(ctx: TestContext) -> None:
    primary = _request(ctx, "GET", "/missing-primary", "primary.test")
    alternate = _request(ctx, "GET", "/missing-alternate", "alternate.test")

    assert_status(primary, 404)
    assert_body_contains(primary, "PRIMARY CUSTOM 404")

    assert_status(alternate, 404)
    assert_body_contains(alternate, "ALTERNATE CUSTOM 404")


def test_upload_enabled_and_disabled(ctx: TestContext) -> None:
    upload_path = ctx.path("tests", "fixtures", "www", "uploads", "contract.txt")

    if os.path.exists(upload_path):
        os.remove(upload_path)

    enabled = _request(
        ctx,
        "POST",
        "/upload/contract.txt",
        "primary.test",
        "integration upload\n",
    )
    disabled = _request(
        ctx,
        "POST",
        "/contract.txt",
        "primary.test",
        "must not be stored\n",
    )

    assert_status(enabled, 201)
    assert_status(disabled, 405)

    if not os.path.isfile(upload_path):
        raise AssertionError("upload-enabled location did not create the file")

    os.remove(upload_path)


def test_traversal_attempt_is_rejected(ctx: TestContext) -> None:
    escaped_path = ctx.path("tests", "fixtures", "www", "escape.txt")

    if os.path.exists(escaped_path):
        os.remove(escaped_path)

    response = _request(
        ctx,
        "POST",
        "/upload/../escape.txt",
        "primary.test",
        "escape attempt\n",
    )

    assert_status(response, 400)

    if os.path.exists(escaped_path):
        raise AssertionError("path traversal created a file outside upload_path")


def register_tests():
    return [
        ("virtual hosts share one listener", test_virtual_servers_share_one_listener),
        ("unknown Host uses default server", test_unknown_host_uses_default_server),
        ("malformed Host returns 400", test_malformed_host_returns_400),
        ("longest location and segment boundary", test_longest_location_and_segment_boundary),
        ("body limit follows selected server", test_body_limit_follows_selected_server),
        ("body limit can be overridden per location", test_body_limit_can_be_overridden_per_location),
        ("custom error page follows selected server", test_custom_error_page_follows_selected_server),
        ("upload enabled and disabled", test_upload_enabled_and_disabled),
        ("traversal attempt is rejected", test_traversal_attempt_is_rejected),
    ]
