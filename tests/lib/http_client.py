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


def send_raw_request_allow_reset(
    host: str,
    port: int,
    request: str,
    timeout: float = 2.0,
) -> str:
    """
    Send a raw request but tolerate connection reset while sending.

    This is useful for tests where the server is allowed to reject early,
    for example oversized bodies returning 413 before the client finishes
    uploading the body.
    """
    sock: socket.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(timeout)
    sock.connect((host, port))

    data: bytes = request.encode("utf-8")
    sent: int = 0

    try:
        while sent < len(data):
            sent += sock.send(data[sent:sent + 4096])
    except socket.error:
        pass

    chunks: list[bytes] = []

    while True:
        try:
            chunk: bytes = sock.recv(4096)
            if not chunk:
                break
            chunks.append(chunk)
        except socket.error:
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
