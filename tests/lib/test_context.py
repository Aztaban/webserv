import os

from tests.lib.http_client import send_raw_request


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
