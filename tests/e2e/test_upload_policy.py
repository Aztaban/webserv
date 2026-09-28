"""Upload permission must be checked independently of allowed HTTP methods."""
import http.client
from pathlib import Path
import socket
import subprocess
import tempfile
import time


def test_upload_policy(ctx):
    with tempfile.TemporaryDirectory(prefix='webserv-upload-policy-') as directory:
        root = Path(directory)
        uploads = root / 'uploads'
        uploads.mkdir()
        (uploads / 'existing.txt').write_bytes(b'original')
        (root / 'visible.txt').write_bytes(b'static content')
        (root / 'forbidden.html').write_bytes(b'Uploads disabled')
        (root / 'echo.py').write_text(
            'import os, sys\n'
            'body = sys.stdin.buffer.read(int(os.environ.get("CONTENT_LENGTH", "0")))\n'
            'sys.stdout.buffer.write(b"Content-Type: application/octet-stream\\r\\n\\r\\n" + body)\n')
        with socket.socket() as reservation:
            reservation.bind(('127.0.0.1', 0))
            port = reservation.getsockname()[1]
        config = root / 'server.conf'
        config.write_text('''server {
 listen 127.0.0.1:PORT;
 root ROOT;
 error_page 403 ROOT/forbidden.html;
 location /off { methods GET POST; upload off; upload_path ROOT/uploads; }
 location /default { methods GET POST; upload_path ROOT/uploads; }
 location /unrestricted { upload off; upload_path ROOT/uploads; }
 location /on { methods POST; upload on; upload_path ROOT/uploads; }
 location /get-only { methods GET; upload on; upload_path ROOT/uploads; }
 location /cgi { methods GET POST; root ROOT; upload off; cgi_extension .py /usr/bin/python3; }
}
'''.replace('PORT', str(port)).replace('ROOT', str(root)))

        def request(method, path, body=None):
            client = http.client.HTTPConnection('127.0.0.1', port, timeout=3)
            try:
                client.request(method, path, body=body)
                response = client.getresponse()
                return response.status, response.read()
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
                        request('GET', '/off/visible.txt')
                        break
                    except OSError:
                        if time.monotonic() >= deadline:
                            raise AssertionError('Upload policy server did not start')
                        time.sleep(.02)
                for route in ('off', 'default', 'unrestricted'):
                    for name in ('new.txt', 'existing.txt'):
                        actual = request('POST', '/' + route + '/' + name, b'replacement')
                        assert actual == (403, b'Uploads disabled'), (route, name, actual)
                    assert request('GET', '/' + route + '/visible.txt') == (200, b'static content')
                assert (uploads / 'existing.txt').read_bytes() == b'original'
                assert not (uploads / 'new.txt').exists()
                assert request('POST', '/get-only/blocked.txt', b'blocked')[0] == 405
                assert not (uploads / 'blocked.txt').exists()
                assert request('POST', '/on/allowed.txt', b'allowed')[0] == 201
                assert (uploads / 'allowed.txt').read_bytes() == b'allowed'
                payload = b'CGI POST\x00\xff'
                assert request('POST', '/cgi/echo.py', payload) == (200, payload)
                assert request('POST', '/cgi/not-a-script.txt', b'blocked')[0] == 403
                assert not (uploads / 'not-a-script.txt').exists()
            finally:
                if server.poll() is None:
                    server.terminate()
                    try:
                        server.wait(timeout=3)
                    except subprocess.TimeoutExpired:
                        server.kill()
                        server.wait()


def register_tests():
    return [('upload off blocks file writes while CGI POST remains available', test_upload_policy)]
