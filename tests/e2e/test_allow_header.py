"""Check route-specific Allow headers, including custom 405 pages."""
import http.client
from pathlib import Path
import socket
import subprocess
import tempfile
import time


def test_allow_header(ctx):
    with tempfile.TemporaryDirectory(prefix='webserv-allow-') as directory:
        root = Path(directory)
        (root / '405.html').write_bytes(b'Custom method error')
        (root / '501.html').write_bytes(b'Custom unsupported method')
        with socket.socket() as reservation:
            reservation.bind(('127.0.0.1', 0))
            port = reservation.getsockname()[1]
        config = root / 'server.conf'
        config.write_text('''server {
 listen 127.0.0.1:PORT;
 root ROOT;
 error_page 405 ROOT/405.html;
 error_page 501 ROOT/501.html;
 location / { methods GET; }
 location /files { methods GET DELETE; }
 location /files/private { methods POST; upload on; }
 location /cgi { methods GET POST; root ROOT; cgi_extension .py /usr/bin/python3; }
}
'''.replace('PORT', str(port)).replace('ROOT', str(root)))

        def request(method, path):
            client = http.client.HTTPConnection('127.0.0.1', port, timeout=2)
            try:
                client.request(method, path, body=b'x' if method == 'POST' else None)
                response = client.getresponse()
                return response.status, response.getheaders(), response.read()
            finally:
                client.close()

        with (root / 'server.log').open('w') as log:
            server = subprocess.Popen([ctx.path('webserv'), str(config)], cwd=ctx.root_dir,
                                      stdout=log, stderr=log)
            try:
                deadline = time.monotonic() + 3
                while True:
                    if server.poll() is not None:
                        raise AssertionError((root / 'server.log').read_text())
                    try:
                        request('GET', '/')
                        break
                    except OSError:
                        if time.monotonic() >= deadline:
                            raise AssertionError('Allow test server did not start')
                        time.sleep(.02)
                for method, path, expected in [
                    ('POST', '/', 'GET'),
                    ('DELETE', '/missing', 'GET'),
                    ('POST', '/files/example', 'GET, DELETE'),
                    ('GET', '/files/private/example', 'POST'),
                    ('DELETE', '/cgi/example.py', 'GET, POST'),
                ]:
                    status, headers, body = request(method, path)
                    assert status == 405, (method, path, status)
                    values = [value for name, value in headers if name.lower() == 'allow']
                    assert values == [expected], (method, path, values, expected)
                    assert body == b'Custom method error'
                for method in ('PUT', 'PATCH', 'OPTIONS', 'BREW'):
                    for path in ('/', '/files/private/example', '/cgi/example.py'):
                        status, headers, body = request(method, path)
                        assert status == 501, (method, path, status)
                        assert body == b'Custom unsupported method'
                        assert not any(name.lower() == 'allow' for name, _ in headers)
                status, headers, _ = request('GET', '/missing')
                assert status == 404
                assert not any(name.lower() == 'allow' for name, _ in headers)
            finally:
                if server.poll() is None:
                    server.terminate()
                    try:
                        server.wait(timeout=3)
                    except subprocess.TimeoutExpired:
                        server.kill()
                        server.wait()


def register_tests():
    return [('405 Allow follows matched location and survives custom pages', test_allow_header)]
