#!/usr/bin/env python3

import socket
import threading
import time
from typing import List

from tests.lib.assertions import assert_http_response
from tests.lib.test_context import TestContext


def test_slow_client_does_not_freeze_server(ctx: TestContext) -> None:
    """
    Keep one client sending the request slowly.

    While the slow client is connected, a normal request should still succeed.
    This checks that one slow client does not block the whole server.
    """
    failures: List[str] = []

    def slow_client() -> None:
        """Send an HTTP request byte-by-byte with small delays."""
        try:
            sock: socket.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(20)
            sock.connect((ctx.host, ctx.port))

            request: str = (
                "GET / HTTP/1.1\r\n"
                "Host: localhost\r\n"
                "Connection: close\r\n"
                "\r\n"
            )

            for ch in request:
                sock.send(ch.encode("utf-8"))
                time.sleep(0.05)

            data: bytes = sock.recv(4096)
            response: str = data.decode("utf-8", errors="replace")
            assert_http_response(response)

            sock.close()

        except Exception as error:
            failures.append("slow client failed: {}".format(error))

        thread: threading.Thread = threading.Thread(target=slow_client)
        thread.start()

        # Give the slow client time to connect before sending a normal request.
        time.sleep(0.5)

        response: str = ctx.get("/", timeout=3)
        assert_http_response(response)

        thread.join()

        if failures:
            raise AssertionError(failures[0])


def register_tests():
    return [
        ("slow client does not freeze server", test_slow_client_does_not_freeze_server)
    ]
