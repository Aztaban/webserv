"""Exercise the real 30-second CGI deadline without shortening production limits."""
import concurrent.futures
import os
from pathlib import Path
import signal
import tempfile
import time

from tests.lib.assertions import assert_status
from tests.lib.http_response import get_body


def test_cgi_deadline_returns_504(ctx):
    with tempfile.TemporaryDirectory(prefix='timeout-', dir=ctx.path('cgi-bin')) as directory:
        root = Path(directory)
        cases = [('get', 'GET', False), ('post', 'POST', False), ('closed', 'GET', True)]
        for name, method, close_output in cases:
            script = (
                'import os, sys, time\n'
                'sys.stdin.read(int(os.environ.get("CONTENT_LENGTH", "0")))\n'
            )
            if close_output:
                script += (
                    'for fd in range(1, 256):\n'
                    '    try: os.close(fd)\n'
                    '    except OSError: pass\n'
                )
            script += (
                'with open(' + repr(str(root / (name + '.pid'))) + ', "w") as f: f.write(str(os.getpid()))\n'
                'while True: time.sleep(1)\n'
            )
            (root / (name + '.py')).write_text(script)
        child_pids = set()
        try:
            with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:
                started = time.monotonic()
                responses = []
                for name, method, close_output in cases:
                    url = '/cgi-bin/' + root.name + '/' + name + '.py'
                    if method == 'POST':
                        responses.append(pool.submit(ctx.post, url, 'timeout-body', 36))
                    else:
                        responses.append(pool.submit(ctx.get, url, 36))
                # All three scripts must actually start before testing the timeout.
                deadline = time.monotonic() + 3
                while len(child_pids) != len(cases):
                    for name, _, _ in cases:
                        marker = root / (name + '.pid')
                        if marker.exists() and marker.read_text().strip():
                            child_pids.add(int(marker.read_text()))
                    if time.monotonic() >= deadline:
                        raise AssertionError('Not all timeout CGI scripts started')
                    time.sleep(.01)
                # Repeatedly prove the server remains responsive while they run.
                while not all(response.done() for response in responses):
                    assert_status(ctx.get('/', timeout=1), 200)
                    if time.monotonic() - started > 37:
                        raise AssertionError('CGI requests exceeded their deadline')
                    time.sleep(.25)
                for (name, _, _), response in zip(cases, responses):
                    raw = response.result()
                    assert_status(raw, 504)
                    if '504' not in get_body(raw):
                        raise AssertionError(name + ': timeout should have an error body')
                elapsed = time.monotonic() - started
                # std::time() has one-second resolution; allow scheduling overhead.
                if not 9 <= elapsed <= 12:
                    raise AssertionError('Unexpected CGI deadline: %.2fs' % elapsed)
            deadline = time.monotonic() + 2
            while child_pids:
                for pid in list(child_pids):
                    try:
                        os.kill(pid, 0)
                    except ProcessLookupError:
                        child_pids.remove(pid)
                if child_pids and time.monotonic() >= deadline:
                    raise AssertionError('Timed-out CGI children were not reaped')
                if child_pids:
                    time.sleep(.02)
            assert_status(ctx.get('/cgi-bin/hello.py?name=after-timeout', timeout=5), 200)
        finally:
            # Clean up this test's children on failure; never target unrelated jobs.
            for pid in child_pids:
                try:
                    os.kill(pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass


def register_tests():
    return [('CGI GET/POST/closed-stdout deadlines return 504 and reap children', test_cgi_deadline_returns_504)]
