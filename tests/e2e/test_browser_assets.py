from tests.lib.assertions import assert_http_response, assert_status
from tests.lib.test_context import TestContext


def test_css_asset_loads(ctx: TestContext) -> None:
    """
    Final expected behavior:
    Browser-linked CSS assets should be served as normal static files.
    """
    response: str = ctx.get("/style.css")

    assert_http_response(response)
    assert_status(response, 200)


def test_js_asset_loads(ctx: TestContext) -> None:
    """
    Final expected behavior:
    Browser-linked JavaScript assets should be served as normal static files.
    """
    response: str = ctx.get("/script.js")

    assert_http_response(response)
    assert_status(response, 200)


def test_image_asset_loads(ctx: TestContext) -> None:
    """
    Final expected behavior:
    Browser-linked image assets should be served as normal static files.
    """
    response: str = ctx.get("/logo.png")

    assert_http_response(response)
    assert_status(response, 200)


def register_tests():
    return [
        ("CSS asset loads", test_css_asset_loads),
        ("JS asset loads", test_js_asset_loads),
        ("image asset loads", test_image_asset_loads),
    ]
