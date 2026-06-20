import socket

from tests.lib.assertions import assert_http_response
from tests.lib.test_context import TestContext


def test_connect_disconnect_loop(ctx: TestContext) -> None:
    """
    Repeatedly connect and close immediately.

    This verifies that accept/cleanup logic does not crash the server.
    """
    for _ in range(50):
        sock: socket.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(1)
        sock.connect((ctx.host, ctx.port))
        sock.close()

    response: str = ctx.get("/")
    assert_http_response(response)


def test_partial_request_close_loop(ctx: TestContext) -> None:
    """
    Send incomplete requests and close the socket.

    This checks that clients disconnecting mid-request are cleaned safely.
    """
    for _ in range(50):
        sock: socket.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(1)
        sock.connect((ctx.host, ctx.port))
        sock.sendall(b"GET / HTTP/1.1\r\nHost: localhost\r\n")
        sock.close()

    response: str = ctx.get("/")
    assert_http_response(response)


def register_tests():
    return [
        ("connect/disconnect loop", test_connect_disconnect_loop),
        ("partial request close loop", test_partial_request_close_loop),
    ]
