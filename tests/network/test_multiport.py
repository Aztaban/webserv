from tests.lib.assertions import assert_http_response
from tests.lib.test_context import TestContext


def test_primary_port_responds(ctx: TestContext) -> None:
    """Verify that the primary listener port responds."""
    response: str = ctx.get_on_port(ctx.port, "/")
    assert_http_response(response)


def test_second_port_responds(ctx: TestContext) -> None:
    """
    Verify that the second listener port responds.

    For now this may rely on the temporary hardcoded second listener.
    Later it should pass because test.conf contains a second listen directive.
    """
    response: str = ctx.get_on_port(ctx.second_port, "/")
    assert_http_response(response)


def test_both_ports_stay_alive(ctx: TestContext) -> None:
    """Send requests to both ports and verify both still respond."""
    primary_response: str = ctx.get_on_port(ctx.port, "/")
    second_response: str = ctx.get_on_port(ctx.second_port, "/")

    assert_http_response(primary_response)
    assert_http_response(second_response)


def register_tests():
    return [
        ("primary port responds", test_primary_port_responds),
        ("second port responds", test_second_port_responds),
        ("both ports stay alive", test_both_ports_stay_alive),
    ]
