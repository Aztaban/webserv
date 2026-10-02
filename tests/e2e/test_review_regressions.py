"""Regressions from the subject review: blocking files, CGI framing and URI paths."""
from contextlib import contextmanager
import fcntl
import os
from pathlib import Path
import socket
import subprocess
import tempfile
import time
from urllib.parse import quote


@contextmanager
def review_server(ctx, piped_logs=False):
    with tempfile.TemporaryDirectory(prefix='webserv-review-') as directory:
        root = Path(directory)
        (root / 'uploads').mkdir()
        (root / 'index.html').write_bytes(b'healthy')
        (root / 'echo.py').write_text('import sys\nbody = sys.stdin.buffer.read()\nsys.stdout.buffer.write(b"Content-Type: text/plain\\r\\nStatus: 201 Created\\r\\nX-Test:yes\\r\\ncontent-length: 999\\r\\n\\r\\n" + body)\n')
        with socket.socket() as reservation:
            reservation.bind(('127.0.0.1', 0))
            port = reservation.getsockname()[1]
        config = root / 'server.conf'
        config.write_text(f'''server {{
 listen 127.0.0.1:{port};
 root {root};
 client_max_body_size 32;
 location / {{ methods GET POST DELETE; autoindex on; upload on;
  upload_path {root}/uploads; cgi_extension .py /usr/bin/python3; }}
 location /listing {{ methods GET; root {root}/uploads; autoindex on; }}
}}''')

        def request(path='/', method='GET', body=b'', headers='', timeout=2):
            if body and not headers:
                headers = 'Content-Length: %d\r\n' % len(body)
            with socket.create_connection(('127.0.0.1', port), timeout=timeout) as client:
                client.sendall(f'{method} {path} HTTP/1.1\r\nHost: localhost\r\n{headers}\r\n'.encode() + body)
                result = b''
                while True:
                    block = client.recv(65536)
                    if not block:
                        return result
                    result += block

        with (root / 'server.log').open('wb') as log:
            destination = subprocess.PIPE if piped_logs else log
            server = subprocess.Popen([ctx.path('webserv'), str(config)], cwd=ctx.root_dir,
                                      stdout=destination, stderr=destination)
            try:
                deadline = time.monotonic() + 3
                while True:
                    if server.poll() is not None:
                        raise AssertionError('Review server failed to start')
                    try:
                        assert status(request()) == 200
                        break
                    except OSError:
                        if time.monotonic() >= deadline:
                            raise
                        time.sleep(.02)
                yield root, request, server
            finally:
                if server.poll() is None:
                    server.terminate()
                    try:
                        server.wait(timeout=3)
                    except subprocess.TimeoutExpired:
                        server.kill()
                        server.wait()
                if piped_logs:
                    server.stdout.close()
                    server.stderr.close()


def status(raw):
    return int(raw.split(b' ', 2)[1])


def payload(raw):
    return raw.split(b'\r\n\r\n', 1)[1]


def test_regular_files_only(ctx):
    assert os.geteuid() != 0, 'Run file-permission regressions as an unprivileged user'
    with review_server(ctx) as (root, request, _):
        os.mkfifo(root / 'blocked')
        os.mkfifo(root / 'uploads' / 'blocked')
        (root / 'errors').mkdir()
        os.mkfifo(root / 'errors' / '404.html')
        (root / 'unreadable').write_bytes(b'secret')
        (root / 'unreadable').chmod(0)
        (root / 'empty').touch()
        try:
            assert status(request('/blocked')) == 403
            assert status(request('/blocked', 'POST', b'payload')) == 500
            assert status(request('/missing')) == 404  # fallback must not open the FIFO
            assert status(request('/unreadable')) == 403
            empty = request('/empty')
            assert status(empty) == 200 and payload(empty) == b''
            assert payload(request()) == b'healthy'
        finally:
            (root / 'unreadable').chmod(0o600)


def test_cgi_headers_and_decoded_limits(ctx):
    with review_server(ctx) as (_, request, _):
        bodies = [
            (b'x' * 32, 'Content-Length: 32\r\n'),
            (b'20\r\n' + b'x' * 32 + b'\r\n0\r\n\r\n', 'Transfer-Encoding: chunked\r\n'),
            (b'1;extension=value\r\nx\r\n' * 32 + b'0\r\nX-Trailer: yes\r\n\r\n',
             'Transfer-Encoding: chunked\r\n'),
        ]
        for body, headers in bodies:
            raw = request('/echo.py', 'POST', body, headers)
            assert status(raw) == 201, raw
            head = raw.split(b'\r\n\r\n')[0]
            assert b'\n' not in head.replace(b'\r\n', b''), raw
            assert b'X-Test: yes\r\n' in raw, raw
            assert b'Content-Length: 32\r\n' in raw, raw
            assert raw.lower().count(b'content-length:') == 1, raw
            assert payload(raw) == b'x' * 32
        # Reject an oversized declared chunk without waiting for its data.
        assert status(request('/echo.py', 'POST', b'21\r\n', 'Transfer-Encoding: chunked\r\n')) == 413
        assert status(request('/echo.py', 'POST', b'', 'Content-Length: 33\r\n')) == 413
        assert status(request('/echo.py', 'POST', b'1\r\nx\r\n20\r\n', 'Transfer-Encoding: chunked\r\n')) == 413
        assert payload(request('/echo.py', 'POST', b'0\r\n\r\n', 'Transfer-Encoding: chunked\r\n')) == b''
        # Exercise the same limit on file uploads, not just CGI input.
        raw = request('/upload.bin', 'POST', bodies[1][0], bodies[1][1])
        assert status(raw) == 201
        assert payload(request('/uploads/upload.bin')) == b'x' * 32


def test_encoded_paths_and_listing(ctx):
    with review_server(ctx) as (root, request, _):
        name = 'hello world?#%+.txt'
        encoded = quote(name, safe='')
        raw = request('/' + encoded, 'POST', b'exact bytes')
        assert status(raw) == 201
        assert (root / 'uploads' / name).read_bytes() == b'exact bytes'
        assert payload(request('/uploads/' + encoded + '?ignored=yes')) == b'exact bytes'
        listing = payload(request('/listing/')).decode()
        assert 'href="/listing/' + encoded + '"' in listing, listing
        assert payload(request('/listing/' + encoded)) == b'exact bytes'
        assert status(request('/uploads/' + encoded, 'DELETE')) == 204
        assert not (root / 'uploads' / name).exists()
        for path in ('/%', '/%GG', '/%00', '/%0d%0a'):
            assert status(request(path)) == 400, path
        assert status(request('/%2e%2e/server.conf')) in (403, 404)
        (root / 'percent%20literal').write_bytes(b'only decoded once')
        assert payload(request('/percent%2520literal')) == b'only decoded once'


def test_undrained_logging_pipe(ctx):
    with review_server(ctx, piped_logs=True) as (_, request, server):
        if hasattr(fcntl, 'F_SETPIPE_SZ'):
            fcntl.fcntl(server.stdout.fileno(), fcntl.F_SETPIPE_SZ, 4096)
            fcntl.fcntl(server.stderr.fileno(), fcntl.F_SETPIPE_SZ, 4096)
        for _ in range(200):
            assert status(request()) == 200
        for stream in (server.stdout, server.stderr):
            os.set_blocking(stream.fileno(), False)
            try:
                output = os.read(stream.fileno(), 4096)
            except BlockingIOError:
                output = b''
            assert output == b'', 'Server wrote to an unpolled logging pipe'


def register_tests():
    return [
        ('FIFOs never block GET/upload/error pages and unreadable files return errors', test_regular_files_only),
        ('CGI CRLF headers and decoded chunk limits preserve exact payloads', test_cgi_headers_and_decoded_limits),
        ('encoded filenames round-trip through upload, listing, GET and DELETE', test_encoded_paths_and_listing),
        ('undrained logging pipes never stall active HTTP requests', test_undrained_logging_pipe),
    ]
