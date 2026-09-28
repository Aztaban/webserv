#!/usr/bin/env python3
"""Local Chromium, finite endurance and Valgrind checks; artifacts in /tmp."""
import json
import os
from pathlib import Path
import shutil
import signal
import subprocess
import tempfile
import time
from evaluation_audit import ROOT, setup, request

base = setup()
print('Artifacts: ' + str(base), flush=True)
log = (base / 'server.log').open('w')
server = subprocess.Popen([str(ROOT / 'webserv'), str(base / 'audit.conf')], cwd=ROOT,
                          stdout=log, stderr=log)
try:
    for _ in range(100):
        try:
            request(); break
        except OSError:
            if server.poll() is not None: raise RuntimeError('server startup failed')
            time.sleep(.05)
    chrome = '/snap/chromium/current/usr/lib/chromium-browser/chrome'
    if not Path(chrome).exists():
        raise RuntimeError('Set chrome to an installed Chromium executable')
    browser_results = []
    cases = [('index', 'http://127.0.0.1:18880/', 'CONFIGURED INDEX'),
             ('missing', 'http://127.0.0.1:18880/missing', 'CUSTOM ERROR MARKER'),
             ('listing', 'http://127.0.0.1:18880/listing/', 'a.txt'),
             ('redirect', 'http://127.0.0.1:18880/old', 'CONFIGURED INDEX'),
             ('port', 'http://127.0.0.1:18881/', 'SECOND SITE'),
             ('interface', 'http://127.0.0.2:18880/', 'SECOND SITE')]
    for name, url, marker in cases:
        args = [chrome, '--headless', '--no-sandbox', '--disable-gpu', '--disable-background-networking', '--no-proxy-server',
                '--user-data-dir=' + str(base / 'browser-profile'), '--dump-dom',
                '--log-net-log=' + str(base / (name + '-network.json')), url]
        result = subprocess.run(args, capture_output=True, text=True, timeout=20)
        (base / (name + '.html')).write_text(result.stdout)
        (base / (name + '-browser.log')).write_text(result.stderr)
        assert result.returncode == 0 and marker in result.stdout, (name, result.stderr)
        browser_results.append({'case': name, 'passed': True})
    (base / 'browser-results.json').write_text(json.dumps(browser_results, indent=2))
    print('Chromium: 6/6 rendered pages passed; network logs captured', flush=True)
    siege = shutil.which('siege') or '/tmp/webserv-siege-tool/unpacked/usr/bin/siege'
    args = [siege]
    if siege.startswith('/tmp/'):
        args += ['-R', '/tmp/webserv-siege-tool/unpacked/etc/siege/siegerc']
    samples = []
    started = time.monotonic()
    for i in range(12):
        round_start = time.monotonic()
        result = subprocess.run(args + ['-b', '-c', '20', '-r', '100',
                                      'http://127.0.0.1:18880/empty.html'],
                                capture_output=True, text=True, timeout=30)
        output = result.stdout + result.stderr
        (base / ('endurance-%02d.log' % i)).write_text(output)
        data = json.loads(output[output.index('{'):output.rindex('}') + 1])
        assert result.returncode == 0 and data['availability'] > 99.5, data
        time.sleep(.1)
        status = Path('/proc/%d/status' % server.pid).read_text()
        sample = {'round': i + 1, 'elapsed': round(time.monotonic() - started, 2),
                  'rss_kb': int(status.split('VmRSS:')[1].split()[0]),
                  'fds': len(list(Path('/proc/%d/fd' % server.pid).iterdir())),
                  'transactions': data['transactions'], 'availability': data['availability']}
        samples.append(sample)
        print(json.dumps(sample), flush=True)
        time.sleep(max(0, 10 - (time.monotonic() - round_start)))
    assert request('/empty.html')[0] == 200
    (base / 'endurance.json').write_text(json.dumps(samples, indent=2))
finally:
    server.terminate()
    server.wait(timeout=5)
    log.close()

with (base / 'valgrind-server.log').open('w') as log:
    server = subprocess.Popen(['valgrind', '--leak-check=full', '--show-leak-kinds=all',
                               '--log-file=' + str(base / 'valgrind.log'),
                               str(ROOT / 'webserv'), str(base / 'audit.conf')],
                              cwd=ROOT, stdout=log, stderr=log)
    try:
        for _ in range(150):
            try:
                request(); break
            except OSError:
                if server.poll() is not None: raise RuntimeError('Valgrind server failed')
                time.sleep(.05)
        for _ in range(30):
            assert request('/empty.html')[0] == 200
            assert request('/missing')[0] == 404
        assert request('/cgi-bin/relative.py')[0] == 200
        time.sleep(.3)
    finally:
        server.send_signal(signal.SIGINT)
        server.wait(timeout=10)
text = (base / 'valgrind.log').read_text()
assert 'ERROR SUMMARY: 0 errors' in text, text
assert 'definitely lost: 0 bytes' in text or 'All heap blocks were freed' in text, text
print('Valgrind: no errors or definitely lost blocks; inspect log for reachable shutdown allocations', flush=True)
