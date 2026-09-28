"""Check directory listing policy and links through real configured routes."""
import http.client
from pathlib import Path
import socket
import subprocess
import tempfile
import time


def test_autoindex_routes(ctx):
    with tempfile.TemporaryDirectory(prefix='webserv-autoindex-') as directory:
        root = Path(directory)
        for name in ('public/sub', 'private/sub', 'indexed', 'directory-index/home.html'):
            (root / name).mkdir(parents=True)
        (root / 'public/file.txt').write_text('PUBLIC FILE')
        (root / 'public/sub/child.txt').write_text('CHILD FILE')
        (root / 'private/secret.txt').write_text('PRIVATE FILE')
        (root / 'private/sub/secret.txt').write_text('PRIVATE CHILD')
        (root / 'indexed/home.html').write_text('INDEX WINS')
        (root / 'indexed/extra.txt').write_text('NOT A LISTING')
        with socket.socket() as reservation:
            reservation.bind(('127.0.0.1', 0))
            port = reservation.getsockname()[1]
        config = root / 'autoindex.conf'
        config.write_text('''server {
 listen 127.0.0.1:PORT;
 root ROOT/public;
 index home.html;
 location / { methods GET; autoindex on; }
 location /listing { methods GET; root ROOT/public; autoindex on; }
 location /private { methods GET; root ROOT/private; autoindex off; }
 location /default { methods GET; root ROOT/private; }
 location /indexed-on { methods GET; root ROOT/indexed; autoindex on; }
 location /indexed-off { methods GET; root ROOT/indexed; autoindex off; }
 location /directory-index { methods GET; root ROOT/directory-index; autoindex off; }
}
'''.replace('PORT', str(port)).replace('ROOT', str(root)))
        def get(path):
            client = http.client.HTTPConnection('127.0.0.1', port, timeout=2)
            try:
                client.request('GET', path)
                response = client.getresponse()
                return response.status, response.read().decode()
            finally:
                client.close()
        with (root / 'server.log').open('w') as log:
            server = subprocess.Popen([ctx.path('webserv'), str(config)], cwd=ctx.root_dir,
                                      stdout=log, stderr=log)
            try:
                deadline = time.monotonic() + 3
                while True:
                    if server.poll() is not None:
                        raise AssertionError('Autoindex server failed: ' + (root / 'server.log').read_text())
                    try:
                        get('/file.txt')
                        break
                    except OSError:
                        if time.monotonic() >= deadline:
                            raise AssertionError('Autoindex server did not start')
                        time.sleep(.02)
                failures = []
                for path, base, filename in [('/', '/', 'file.txt'),
                    ('/listing', '/listing/', 'file.txt'),
                    ('/listing/', '/listing/', 'file.txt'),
                    ('/listing/?sort=name', '/listing/', 'file.txt'),
                    ('/listing/sub/', '/listing/sub/', 'child.txt')]:
                    code, body = get(path)
                    if code != 200 or 'Index of ' + base not in body:
                        failures.append(path + ': expected a directory listing, got ' + str(code))
                    if 'href="' + base + filename + '"' not in body:
                        failures.append(path + ': file link lost its public route')
                    if get(base + filename)[0] != 200:
                        failures.append(path + ': listed file cannot be retrieved')
                for path in ['/private', '/private/', '/private/sub/', '/default/sub/', '/directory-index/']:
                    code, body = get(path)
                    if code != 404 or 'secret.txt' in body or 'Index of ' in body:
                        failures.append(path + ': listing must be disabled, got ' + str(code))
                for path in ['/indexed-on/', '/indexed-off/']:
                    if get(path) != (200, 'INDEX WINS'):
                        failures.append(path + ': configured index should take priority')
                if get('/private/secret.txt') != (200, 'PRIVATE FILE'):
                    failures.append('autoindex off must not forbid explicit file GET')
                if get('/missing/')[0] != 404:
                    failures.append('missing directory must return 404')
                code, body = get('/listing/')
                if code == 200 and 'href="/listing/sub/"' not in body:
                    failures.append('directory link needs its route and trailing slash')
                if failures:
                    raise AssertionError('\n'.join(failures))
            finally:
                if server.poll() is None:
                    server.terminate()
                    try: server.wait(timeout=3)
                    except subprocess.TimeoutExpired:
                        server.kill()
                        server.wait()


def register_tests():
    return [('autoindex respects route policy and produces usable directory links', test_autoindex_routes)]
