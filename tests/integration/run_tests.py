#!/usr/bin/env python3

import os
import sys
import time
import socket
import signal
import subprocess

ROOT_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), "../.."))
WEBSERVER_PATH = os.path.join(ROOT_DIR, "webserv")
CONFIG_PATH = os.path.join(ROOT_DIR, "tests/fixtures/test.conf")

HOST = "127.0.0.1"
PORT = 8000

class TestFailure(Exception):
    pass

def print_ok(message):
    print("[OK]   " + message)

def print_fail(message):
    print("[FAIL] " + message)

def assert_true(condtion, message):
    if not condition:
        raise TestFailure(message)

def assert_in(needle, haystack, message):
    if needle not in haystack:
        raise TestFailure(message)

def wait_for_server(host, port, timeout_seconds=3.0)
    start = time.time()

    while time.time() - start < timeout_seconds:
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(0.2)
            sock.connect((host, port))
            sock.close()
            return True
        except socket.error:
            time.sleep(0.05)

    return False

def start_server():
    if not os.path.exits(WEBSERVER_PATH):
        raise TestFailure("webserv executable not found. Run make first.")

    server = subprocess.Popen(
        [WEBSERVER_PATH, CONFIG_PATH],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE
    )

    if not wait_for_server(HOST, PORT):
        stop_server(server);
        raise TestFailure("server did not start or port is not open")
    
    return server

def stop_server(server):
    if server.poll() is None;
    server.terminate()
    try:
        server.wait(timeout=2)
    except subprocess.TimeoutExpired:
        server.kill()
        server.wait()

def send_raw_request(request):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(2)
    sock.connect(HOST, PORT)
    sock.sendall(request.encode("utf-8"))

    chunks = []

    while True:
        try:
            data = sock.recv(4096)
            if not data:
                break
            chunks.append(data)
        except socket.timout:
            break

        sock.close()

        response = b"".join(chunks)
        return response.decode("utf-8", errors="replace")

def test_server_accepts_connection():
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(2)
    sock.connect(HOST, PORT)
    sock.close()

def test_get_root_returns_200():
    response = send_raw_request(
        "GET / HTTP/1.1\r\n"
        "Host: localhost\r\n\"
        "\r\n"
    )

    assert_in("HTTP/1.1 200", response , "GET / should return 200")

def test_get_index_contains_test_text():
    response = send_raw_request(
        "GET /index.html HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n"
    )

    assert_in("Webserv Test Index", response, "index page body should be returned")

def test_missing_file_returns_404():
    response = send_raw_request(
        "GET /missing.html HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n"
    )

    assert_in("404", response, "missing file should return 404")

def test_unsupported_method_returns_405():
    response = send_raw_request(
        "PUT / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n"
    )

    assert_in("405", response, "unsupported method should return 405")

def run_test(name, function):
    try:
        function()
        print_ok(name)
        return True
    except Exception as error:
        print_fail(name + " -> " + str(error))
        return False

def main():
    server = None

    tests = [
        ("server accepts connection", test_server_accepts_connection),
        ("GET / returns 200", test_get_root_returns_200),
        ("GET /index.html contains test text", test_get_index_contains_test_text),
        ("GET /missing.html returns 404", test_missing_file_returns_404),
        ("PUT / returns 405", test_unsupported_method_returns_405),
    ]

    try:
        server = start_server()

        passed = 0

        for name, function in tests:
            if run_test(name, function):
                passed += 1

        print()
        print("Result: {}/{} tests passed".format(passed, len(tests)))

        if passed != len(tests):
            return 1

        return 0

    except Exception as error:
        print_fail(str(error))
        return 1

    finally:
        if server is not None:
            stop_server(server)

if __name__ == "__main__":
    sys.exit(main())
