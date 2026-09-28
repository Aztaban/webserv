"""Verify configured upload storage and retrieval using a real isolated server."""
import http.client
import os
from pathlib import Path
import socket
import subprocess
import tempfile
import time


def test_upload_path(ctx):
    with tempfile.TemporaryDirectory(prefix='webserv-upload-path-') as directory:
        base = Path(directory)
        site = base / 'site'
        (site / 'uploads').mkdir(parents=True)
        storage = base / 'storage'
        storage.mkdir()
        alternate = base / 'alternate'
        (alternate / 'uploads').mkdir(parents=True)
        (base / 'not-directory').write_text('file')
        with socket.socket() as reservation:
            reservation.bind(('127.0.0.1', 0))
            port = reservation.getsockname()[1]
        config = base / 'server.conf'
        config.write_text('''server {
 listen 127.0.0.1:%s;
 server_name localhost;
 root %s;
 location / { methods GET; }
 location /absolute { methods POST; upload on; upload_path %s; }
 location /trailing { methods POST; upload on; upload_path %s/; }
 location /relative { methods POST; upload on; upload_path %s; }
 location /alternate { methods POST; upload on; root %s; upload_path %s; }
 location /fallback { methods POST; upload on; root %s; }
 location /missing { methods POST; upload on; upload_path %s/missing; }
 location /invalid { methods POST; upload on; upload_path %s/not-directory; }
 location /files { methods GET; root %s; }
}
''' % (port, site, storage, storage, os.path.relpath(storage, ctx.root_dir),
       alternate, storage, alternate, base, base, storage))

        def request(method, path, body=None):
            client = http.client.HTTPConnection('127.0.0.1', port, timeout=2)
            try:
                client.request(method, path, body=body)
                response = client.getresponse()
                return response.status, response.read()
            finally:
                client.close()

        with (base / 'server.log').open('w') as log:
            server = subprocess.Popen([ctx.path('webserv'), str(config)], cwd=ctx.root_dir,
                                      stdout=log, stderr=log)
            try:
                deadline = time.monotonic() + 3
                while True:
                    if server.poll() is not None:
                        raise AssertionError((base / 'server.log').read_text())
                    try:
                        request('GET', '/')
                        break
                    except OSError:
                        if time.monotonic() >= deadline:
                            raise AssertionError('Upload test server did not start')
                        time.sleep(.02)
                payload = b'upload bytes\x00\xff\r\n'
                for route in ('absolute', 'trailing', 'relative', 'alternate'):
                    name = route + '.bin'
                    status, _ = request('POST', '/' + route + '/' + name, payload)
                    assert status == 201, (route, status)
                    assert (storage / name).exists(), 'upload_path ignored: ' + route
                    assert (storage / name).read_bytes() == payload
                    assert request('GET', '/files/' + name) == (200, payload)
                assert request('POST', '/absolute', payload)[0] == 201
                assert (storage / 'absolute').read_bytes() == payload
                assert request('POST', '/fallback/default.bin', payload)[0] == 201
                assert (alternate / 'uploads/default.bin').read_bytes() == payload
                for route in ('missing', 'invalid'):
                    assert request('POST', '/' + route + '/failed.bin', payload)[0] == 500
                assert list((site / 'uploads').iterdir()) == []
                assert not (base / 'missing').exists()
                assert sorted(p.name for p in (alternate / 'uploads').iterdir()) == ['default.bin']
            finally:
                if server.poll() is None:
                    server.terminate()
                    try:
                        server.wait(timeout=3)
                    except subprocess.TimeoutExpired:
                        server.kill()
                        server.wait()


def register_tests():
    return [('upload_path controls storage, retrieval and fallback', test_upload_path)]
