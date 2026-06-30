import socket

from tests.lib.assertions import assert_http_response


def test_server_accepts_connection(ctx) -> None:
    """Verify that the server accepts a plain TCP connection."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(2)
    sock.connect((ctx.host, ctx.port))
    sock.close()


def test_basic_get_returns_http_response(ctx) -> None:
    """Verify that a simple GET request returns an HTTP-like response."""
    response = ctx.get("/")
    assert_http_response(response)


def register_tests():
    return [
        ("server accepts connection", test_server_accepts_connection),
        ("basic GET returns HTTP response", test_basic_get_returns_http_response),
    ]
