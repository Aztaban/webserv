#!/usr/bin/env python3
"""Render the bundled site in isolated Chromium and retain local HTTP evidence."""
import json
from pathlib import Path
import socket
import subprocess
import tempfile
import time

root = Path(__file__).resolve().parents[2]
base = Path(tempfile.mkdtemp(prefix='webserv-browser-site-'))
with socket.socket() as reservation:
    reservation.bind(('127.0.0.1', 0))
    port = reservation.getsockname()[1]
config = base / 'site.conf'
config.write_text((root / 'conf/default.conf').read_text().replace('127.0.0.1:8001', '127.0.0.1:' + str(port)))
with (base / 'server.log').open('w') as log:
    server = subprocess.Popen([str(root / 'webserv'), str(config)], cwd=root, stdout=log, stderr=log)
    try:
        for _ in range(100):
            try:
                with socket.create_connection(('127.0.0.1', port), timeout=1): break
            except OSError:
                if server.poll() is not None: raise RuntimeError('server failed')
                time.sleep(.02)
        command = ['/snap/chromium/current/usr/lib/chromium-browser/chrome', '--headless',
                   '--no-sandbox', '--disable-gpu', '--disable-background-networking',
                   '--no-proxy-server', '--user-data-dir=' + str(base / 'profile'),
                   '--log-net-log=' + str(base / 'network.json'), '--dump-dom',
                   '--screenshot=' + str(base / 'site.png'), '--window-size=1280,900',
                   'http://127.0.0.1:' + str(port) + '/']
        result = subprocess.run(command, capture_output=True, text=True, timeout=25)
        (base / 'rendered.html').write_text(result.stdout)
        (base / 'browser.log').write_text(result.stderr)
        assert result.returncode == 0 and 'Webserv Test Orbit' in result.stdout
        data = json.loads((base / 'network.json').read_text())
        types = {v: k for k, v in data['constants']['logEventTypes'].items()}
        transactions = {}
        for event in data['events']:
            kind = types[event['type']]
            params = event.get('params', {})
            ident = event['source']['id']
            if kind == 'HTTP_TRANSACTION_SEND_REQUEST_HEADERS' and ('Host: 127.0.0.1:' + str(port)) in params['headers']:
                transactions[ident] = {'request': params['line'], 'request_headers': params['headers']}
            if kind == 'HTTP_TRANSACTION_READ_RESPONSE_HEADERS' and ident in transactions:
                transactions[ident]['response_headers'] = params['headers']
        records = list(transactions.values())
        for path in ('/', '/assets/styles.css'):
            assert any(r['request'].startswith('GET ' + path + ' ') and r.get('response_headers', [''])[0] == 'HTTP/1.1 200 OK' for r in records), records
        (base / 'local-http.json').write_text(json.dumps(records, indent=2))
        print('PASS: demo site and stylesheet rendered; screenshot and HTTP headers at ' + str(base))
    finally:
        server.terminate()
        server.wait(timeout=5)
