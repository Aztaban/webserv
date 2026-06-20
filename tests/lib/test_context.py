import os

from tests.lib.http_client import send_raw_request


class TestContext:
    """
    Shared context passed into every test.

    Tests should use this instead of hardcoding paths/host/port everywhere.
    """

    def __init__(self, root_dir: str, host: str, port: int, config_path: str) -> None:
        self.root_dir = root_dir
        self.host = host
        self.port = port
        self.config_path = config_path

    def path(self, *parts: str) -> str:
        return os.path.join(self.root_dir, *parts)

    def send_raw_request(self, request: str, timeout: float = 2.0) -> str:
        return send_raw_request(self.host, self.port, request, timeout)
