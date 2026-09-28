"""Exercise failed writes/flushes and unlink permissions over real HTTP."""
import http.client
import os
from pathlib import Path
import socket
import subprocess
import tempfile
import time


def test_file_failures(ctx):
    if os.geteuid() == 0:
        raise AssertionError('Run file-permission regression as an unprivileged user')
    with tempfile.TemporaryDirectory(prefix='webserv-file-failures-') as directory:
        root = Path(directory)
        uploads = root / 'uploads'
        uploads.mkdir()
        locked = root / 'locked'
        locked.mkdir()
        target = locked / 'keep.txt'
        target.write_bytes(b'keep me')
        (uploads / 'full').symlink_to('/dev/full')
        (root / '500.html').write_bytes(b'File operation failed')
        with socket.socket() as reservation:
            reservation.bind(('127.0.0.1', 0))
            port = reservation.getsockname()[1]
        config = root / 'server.conf'
        config.write_text('''server {
 listen 127.0.0.1:PORT;
 root ROOT;
 error_page 500 ROOT/500.html;
 location /upload { methods POST; upload on; upload_path ROOT/uploads; }
 location /locked { methods DELETE; root ROOT/locked; }
 location /files { methods GET DELETE; root ROOT/uploads; }
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
                        request('GET', '/files/missing')
                        break
                    except OSError:
                        if time.monotonic() >= deadline:
                            raise AssertionError('File failure test server did not start')
                        time.sleep(.02)
                errors = []
                # Small bodies expose buffered close/flush errors; large ones fail during write.
                for body in (b'x', b'x' * 32768):
                    actual = request('POST', '/upload/full', body)
                    if actual != (500, b'File operation failed'):
                        errors.append('failed upload reported ' + repr(actual))
                locked.chmod(0o555)
                actual = request('DELETE', '/locked/keep.txt')
                if actual != (500, b'File operation failed'):
                    errors.append('failed deletion reported ' + repr(actual))
                assert target.read_bytes() == b'keep me'
                assert (uploads / 'full').is_symlink()
                assert request('POST', '/upload/ok.txt', b'ok')[0] == 201
                assert request('GET', '/files/ok.txt') == (200, b'ok')
                assert request('DELETE', '/files/ok.txt') == (204, b'')
                assert not (uploads / 'ok.txt').exists()
                assert request('DELETE', '/files/ok.txt')[0] == 404
                assert not errors, '; '.join(errors)
            finally:
                locked.chmod(0o755)
                if server.poll() is None:
                    server.terminate()
                    try:
                        server.wait(timeout=3)
                    except subprocess.TimeoutExpired:
                        server.kill()
                        server.wait()


def register_tests():
    return [('failed upload writes and deletes never report success', test_file_failures)]
