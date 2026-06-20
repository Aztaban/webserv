import socket


def test_server_accepts_connection(ctx):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(2)
    sock.connect((ctx.host, ctx.port))
    sock.close()


def test_basic_get_returns_http_response(ctx):
    response = ctx.send_raw_request(
        "GET / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Connection: close\r\n"
        "\r\n"
    )

    if not response.startswith("HTTP/1.1"):
        raise AssertionError("response should start with HTTP/1.1")


def register_tests():
    return [
        ("server accepts connection", test_server_accepts_connection),
        ("basic GET returns HTTP response", test_basic_get_returns_http_response),
    ]

