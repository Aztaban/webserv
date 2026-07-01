import os
import shutil
from typing import List

from tests.lib.http_client import send_raw_request, send_raw_request_allow_reset


class TestContext:
    """
    Shared context passed into every test.

    Tests should use this instead of hardcoding paths/host/port everywhere.
    """

    def __init__(
        self,
        root_dir: str,
        host: str,
        port: int,
        second_port: int,
        config_path: str,
    ) -> None:
        self.root_dir = root_dir
        self.host = host
        self.port = port
        self.second_port = second_port
        self.config_path = config_path

    def path(self, *parts: str) -> str:
        """Build an absolute path inside the project root."""
        return os.path.join(self.root_dir, *parts)

    def send_raw_request(self, request: str, timeout: float = 2.0) -> str:
        """Send a raw HTTP request to the default test server."""
        return send_raw_request(self.host, self.port, request, timeout)

    def get(self, path: str = "/", timeout: float = 2.0) -> str:
        """Send a simple HTTP GET request."""
        return self.send_raw_request(
            "GET {} HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "Connection: close\r\n"
            "\r\n".format(path),
            timeout=timeout,
        )

    def post(self, path: str, body: str, timeout: float = 5.0) -> str:
        """Send a simple HTTP POST request with Content-Length."""
        return self.send_raw_request(
            "POST {} HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "Content-Length: {}\r\n"
            "Connection: close\r\n"
            "\r\n"
            "{}".format(path, len(body), body),
            timeout=timeout,
        )

    def send_raw_request_to_port(
        self,
        port: int,
        request: str,
        timeout: float = 2.0,
    ) -> str:
        """Send a raw HTTP request to a specific port."""
        return send_raw_request(self.host, port, request, timeout)


    def get_on_port(self, port: int, path: str = "/", timeout: float = 2.0) -> str:
        """Send a GET request to a specific port."""
        return self.send_raw_request_to_port(
            port,
            "GET {} HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "Connection: close\r\n"
            "\r\n".format(path),
            timeout,
        )

    def uploads_dir(self) -> str:
        """Return the test upload directory path."""
        return self.path("tests", "fixtures", "www", "uploads")

    def reset_uploads(self) -> None:
        """
        Clean and recreate the upload fixture directory.

        Use this before upload/delete tests so one test does not affect another.
        """
        upload_dir: str = self.uploads_dir()

        if os.path.isdir(upload_dir):
            shutil.rmtree(upload_dir)

        os.makedirs(upload_dir)

    def write_fixture_file(self, relative_path: str, content: str) -> str:
        """
        Write a file under the project root and return its absolute path.

        Example:
            ctx.write_fixture_file(
                "tests/fixtures/www/uploads/delete_me.txt",
                "delete me\\n"
            )
        """
        path: str = self.path(relative_path)
        directory: str = os.path.dirname(path)

        if directory and not os.path.isdir(directory):
            os.makedirs(directory)

        with open(path, "w") as file:
            file.write(content)

        return path

    def read_file(self, relative_path: str) -> str:
        """Read a text file under the project root."""
        with open(self.path(relative_path), "r", errors="ignore") as file:
            return file.read()

    def upload_files(self) -> List[str]:
        """Return absolute paths of regular files currently in the upload directory."""
        upload_dir: str = self.uploads_dir()

        if not os.path.isdir(upload_dir):
            return []

        files: List[str] = []

        for filename in os.listdir(upload_dir):
            path: str = os.path.join(upload_dir, filename)

            if os.path.isfile(path):
                files.append(path)

        return files

    def upload_contains(self, expected_text: str) -> bool:
        """Return True if any uploaded file contains expected_text."""
        for path in self.upload_files():
            with open(path, "r", errors="ignore") as file:
                if expected_text in file.read():
                    return True

        return False

    def post_allow_reset(self, path: str, body: str, timeout: float = 5.0) -> str:
        """Send POST request but tolerate early server close/reset."""
        return send_raw_request_allow_reset(
            self.host,
            self.port,
            "POST {} HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "Content-Length: {}\r\n"
            "Connection: close\r\n"
            "\r\n"
            "{}".format(path, len(body), body),
            timeout=timeout,
        )
