"""Verify index inheritance and overrides through configuration and real HTTP."""
import http.client
from pathlib import Path
import socket
import subprocess
import tempfile
import time


def test_configured_index(ctx):
    with tempfile.TemporaryDirectory(prefix='webserv-index-') as directory:
        root = Path(directory)
        (root / 'nested').mkdir()
        (root / 'index.html').write_text('DEFAULT INDEX')
        (root / 'home.html').write_text('SERVER INDEX')
        (root / 'welcome.txt').write_text('LOCATION INDEX')
        (root / 'nested/index.html').write_text('WRONG NESTED INDEX')
        (root / 'nested/home.html').write_text('NESTED SERVER INDEX')
        (root / 'nested/welcome.txt').write_text('NESTED LOCATION INDEX')
        with socket.socket() as reservation:
            reservation.bind(('127.0.0.1', 0))
            port = reservation.getsockname()[1]
        config = root / 'index.conf'
        config.write_text('''server {
 listen 127.0.0.1:PORT;
 server_name custom.test;
 root ROOT;
 index home.html;
 location / { methods GET; }
 location /override { methods GET; root ROOT; index welcome.txt; }
}
server {
 listen 127.0.0.1:PORT;
 server_name default.test;
 root ROOT;
 location / { methods GET; }
}
server {
 listen 127.0.0.1:PORT;
 server_name missing.test;
 root ROOT;
 index absent.html;
 location / { methods GET; }
}
'''.replace('PORT', str(port)).replace('ROOT', str(root)))
        def get(path, host='custom.test'):
            client = http.client.HTTPConnection('127.0.0.1', port, timeout=2)
            try:
                client.request('GET', path, headers={'Host': host})
                response = client.getresponse()
                return response.status, response.read().decode(), response.getheader('Content-Type')
            finally:
                client.close()
        with (root / 'server.log').open('w') as log:
            server = subprocess.Popen([ctx.path('webserv'), str(config)], cwd=ctx.root_dir,
                                      stdout=log, stderr=log)
            try:
                deadline = time.monotonic() + 3
                while True:
                    if server.poll() is not None:
                        raise AssertionError('Index test server failed: ' + (root / 'server.log').read_text())
                    try:
                        get('/index.html')
                        break
                    except OSError:
                        if time.monotonic() >= deadline:
                            raise AssertionError('Index test server did not start')
                        time.sleep(.02)
                cases = [
                    ('/', 'custom.test', 'SERVER INDEX', 'text/html'),
                    ('/?query=yes', 'custom.test', 'SERVER INDEX', 'text/html'),
                    ('/nested/', 'custom.test', 'NESTED SERVER INDEX', 'text/html'),
                    ('/override', 'custom.test', 'LOCATION INDEX', 'text/plain'),
                    ('/override/', 'custom.test', 'LOCATION INDEX', 'text/plain'),
                    ('/override/nested/', 'custom.test', 'NESTED LOCATION INDEX', 'text/plain'),
                    ('/index.html', 'custom.test', 'DEFAULT INDEX', 'text/html'),
                    ('/', 'default.test', 'DEFAULT INDEX', 'text/html'),
                ]
                for path, host, body, content_type in cases:
                    actual = get(path, host)
                    expected = (200, body, content_type)
                    if actual != expected:
                        raise AssertionError('%s %s: expected %r, got %r' % (host, path, expected, actual))
                if get('/', 'missing.test')[0] != 404:
                    raise AssertionError('Missing configured index must not silently use index.html')
            finally:
                if server.poll() is None:
                    server.terminate()
                    try:
                        server.wait(timeout=3)
                    except subprocess.TimeoutExpired:
                        server.kill()
                        server.wait()


def register_tests():
    return [('configured index follows server, location and default settings', test_configured_index)]
