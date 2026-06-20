import socket


def send_raw_request(host: str, port: str, request: str, timeout: float = 2.0) -> str:
    """
    Send one raw HTTP request through a socket and return the full response.

    This avoids depending on curl and allows testing partial requests later.
    """
    sock: socket.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(timeout)
    sock.connect((host, port))
    sock.sendall(request.encode("utf-8"))

    chunks: list[bytes] = []

    while True:
        try:
            data: bytes = sock.recv(4096)
            if not data:
                break
            chunks.append(data)
        except socket.timeout:
            break

    sock.close()

    return b"".join(chunks).decode("utf-8", errors="replace")


def can_connect(host: str, port: int, timeout: float = 0.2) -> bool:
    """Return True if a TCP connection to host:port succeeds."""
    try:
        sock: socket.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(timeout)
        sock.connect((host, port))
        sock.close()
        return True
    except socket.error:
        return False
