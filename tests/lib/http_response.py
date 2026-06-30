def get_status_line(response: str) -> str:
    """Return the first response line."""
    return response.split("\r\n", 1)[0]


def get_body(response: str) -> str:
    """Return the response body after the CRLF CRLF separator."""
    parts = response.split("\r\n\r\n", 1)

    if len(parts) != 2:
        return ""

    return parts[1]