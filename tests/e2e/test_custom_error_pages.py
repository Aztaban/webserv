"""Custom error pages must follow the virtual host across error-producing paths."""
import http.client
from pathlib import Path
import socket
import subprocess
import tempfile
import time


def test_custom_error_page_paths(ctx):
    with tempfile.TemporaryDirectory(prefix='webserv-errors-') as directory:
        root = Path(directory)
        for name in ('site/errors', 'different-root', 'pages', 'cgi', 'plain'):
            (root / name).mkdir(parents=True)
        (root / 'site/index.html').write_text('OK')
        (root / 'site/errors/404.html').write_text('LEGACY FALLBACK')
        for host in ('primary', 'alternate'):
            for code in (400, 404, 405, 413, 500):
                (root / ('pages/%s-%s.html' % (host, code))).write_text('%s CUSTOM %s' % (host, code))
        (root / 'pages/empty.html').write_text('')
        (root / 'cgi/error.py').write_text('raise RuntimeError("intentional error")\n')
        (root / 'cgi/bad_output.py').write_text('print("no CGI headers")\n')
        (root / 'cgi/owned_error.py').write_text('print("Status: 404 Not Found\\nContent-Type: text/plain\\ncontent-encoding: identity\\nAllow: GET\\n\\nCGI OWN ERROR")\n')
        with socket.socket() as reservation:
            reservation.bind(('127.0.0.1', 0))
            port = reservation.getsockname()[1]
        blocks = []
        for host in ('primary', 'alternate', 'fallback', 'empty', 'plain'):
            pages = ''
            if host in ('primary', 'alternate'):
                pages = '\n'.join('error_page %s %s/pages/%s-%s.html;' % (code, root, host, code)
                                  for code in (400, 404, 405, 413, 500))
            elif host == 'fallback':
                pages = 'error_page 404 %s/pages/missing.html;' % root
            elif host == 'empty':
                pages = 'error_page 404 %s/pages/empty.html;' % root
            site = root / ('plain' if host == 'plain' else 'site')
            blocks.append('''server {
 listen 127.0.0.1:PORT;
 server_name HOST.test;
 root SITE;
 client_max_body_size 8;
 PAGES
 location / { methods GET; }
 location /other { methods GET; root ROOT/different-root; }
 location /upload { methods POST; root ROOT/site; upload on; }
 location /delete { methods DELETE; root ROOT/site; }
 location /cgi-bin { methods GET; root ROOT/cgi; cgi_extension .py /usr/bin/python3; }
}'''.replace('PORT', str(port)).replace('HOST', host).replace('SITE', str(site))
                .replace('PAGES', pages).replace('ROOT', str(root)))
        config = root / 'errors.conf'
        config.write_text('\n'.join(blocks))
        def get(path, host='primary', method='GET', body=None):
            client = http.client.HTTPConnection('127.0.0.1', port, timeout=3)
            try:
                client.request(method, path, body=body, headers={'Host': host + '.test'})
                response = client.getresponse()
                data = response.read().decode()
                if response.getheader('Content-Length') != str(len(data.encode())):
                    raise AssertionError('Error response Content-Length does not match body')
                if 'CUSTOM' in data:
                    content_types = [value for key, value in response.getheaders() if key.lower() == 'content-type']
                    if content_types != ['text/html'] or response.getheader('Content-Encoding') is not None:
                        raise AssertionError('Custom page retained stale content headers')
                    if path == '/cgi-bin/owned_error.py' and response.getheader('Allow') != 'GET':
                        raise AssertionError('Custom page replacement discarded Allow')
                return response.status, data
            finally:
                client.close()
        def raw_error(host, content_length):
            with socket.create_connection(('127.0.0.1', port), timeout=3) as client:
                client.sendall(('POST /upload HTTP/1.1\r\nHost: '+host+'\r\nContent-Length: '+content_length+'\r\n\r\n').encode())
                data = b''
                while True:
                    chunk = client.recv(4096)
                    if not chunk: break
                    data += chunk
                head, body = data.split(b'\r\n\r\n', 1)
                return int(head.split()[1]), body.decode()
        with (root / 'server.log').open('w') as log:
            server = subprocess.Popen([ctx.path('webserv'), str(config)], cwd=ctx.root_dir, stdout=log, stderr=log)
            try:
                deadline = time.monotonic() + 3
                while True:
                    if server.poll() is not None:
                        raise AssertionError('Error-page server failed: ' + (root / 'server.log').read_text())
                    try: get('/index.html'); break
                    except OSError:
                        if time.monotonic() >= deadline: raise
                        time.sleep(.02)
                failures = []
                def check(label, actual, expected):
                    if actual != expected:
                        failures.append('%s: expected %r, got %r' % (label, expected, actual))
                for host in ('primary', 'alternate'):
                    cases = [('/missing', 'GET', None, 404), ('/other/missing', 'GET', None, 404),
                             ('/missing', 'POST', 'x', 405), ('/delete/missing', 'DELETE', None, 404),
                             ('/upload/file.txt', 'POST', '', 500), ('/upload/file.txt', 'POST', 'x', 500),
                             ('/upload/file.txt', 'POST', 'x'*9, 413), ('/cgi-bin/error.py', 'GET', None, 500),
                             ('/cgi-bin/bad_output.py', 'GET', None, 500), ('/cgi-bin/owned_error.py', 'GET', None, 404)]
                    for path, method, body, code in cases:
                        check(host+' '+method+' '+path, get(path, host, method, body), (code, '%s CUSTOM %s' % (host, code)))
                check('early alternate 413', raw_error('alternate.test','9'), (413, 'alternate CUSTOM 413'))
                check('alternate malformed body length', raw_error('alternate.test','bad'), (400, 'alternate CUSTOM 400'))
                check('malformed Host default page', raw_error('alternate.test:bad','0'), (400, 'primary CUSTOM 400'))
                check('unknown Host default page', get('/missing','unknown'), (404, 'primary CUSTOM 404'))
                check('missing page fallback', get('/missing','fallback'), (404,'LEGACY FALLBACK'))
                check('empty custom page', get('/missing','empty'), (404,''))
                code, body = get('/missing','plain')
                if code != 404 or '404 Not Found' not in body: failures.append('Generated fallback lost error status/body')
                check('CGI body without mapping', get('/cgi-bin/owned_error.py','plain'), (404,'CGI OWN ERROR\n'))
                (root / 'pages/primary-404.html').write_text('EDITED ERROR PAGE')
                check('page edit without restart', get('/missing'), (404,'EDITED ERROR PAGE'))
                check('successful GET unchanged', get('/index.html'), (200,'OK'))
                if failures: raise AssertionError('\n'.join(failures))
            finally:
                if server.poll() is None:
                    server.terminate()
                    try: server.wait(timeout=3)
                    except subprocess.TimeoutExpired: server.kill(); server.wait()


def register_tests():
    return [('custom error-page paths follow host across static/parser/CGI errors', test_custom_error_page_paths)]
