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

