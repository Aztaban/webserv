#!/usr/bin/env python3

import socket
import threading
from typing import List

CLIENTS = 100


def worker(host: str, port: int, index: int, failures: List[str]) -> None:
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(3)
        s.connect((host, port))
        request = (
            "GET / HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "Connection: close\r\n\r\n"
        )
        s.sendall(request.encode())
        data = s.recv(4096)
        if not data.startswith(b"HTTP/1.1"):
            failures.append("client {} received a bad response".format(index))
        s.close()
    except Exception as error:
        failures.append("client {} failed: {}".format(index, error))


def test_many_clients(ctx) -> None:
    failures: List[str] = []
    threads: List[threading.Thread] = []

    for i in range(CLIENTS):
        thread = threading.Thread(target=worker, args=(ctx.host, ctx.port, i, failures))
        threads.append(thread)
        thread.start()

    for thread in threads:
        thread.join()

    if failures:
        raise AssertionError("\n".join(failures))


def register_tests():
    return [
        ("many clients receive HTTP responses", test_many_clients),
    ]


if __name__ == "__main__":
    class ManualContext:
        host = "127.0.0.1"
        port = 8000

    test_many_clients(ManualContext())
    print("done")
