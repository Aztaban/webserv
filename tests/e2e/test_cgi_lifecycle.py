"""CGI EOF and child exit must be handled separately without blocking clients."""
import concurrent.futures
import os
import signal
import socket
import struct
from pathlib import Path
import tempfile
import time

from tests.lib.assertions import assert_status
from tests.lib.http_response import get_body


def _closed_output_test(ctx, method='GET', exit_code=0):
    with tempfile.TemporaryDirectory(prefix='lifecycle-', dir=ctx.path('cgi-bin')) as directory:
        root = Path(directory)
        marker = root / 'ready'
        script = root / 'close_sleep.py'
        script.write_text(
            'import os, sys, time\n'
            'body = sys.stdin.read(int(os.environ.get("CONTENT_LENGTH", "0")))\n'
            'os.write(1, b"Content-Type: text/plain\\n\\n" + ("complete:" + body).encode())\n'
            'for fd in range(1, 256):\n'
            '    try: os.close(fd)\n'
            '    except OSError: pass\n'
            'with open(' + repr(str(marker)) + ', "w") as f: f.write(str(os.getpid()))\n'
            'time.sleep(2)\n'
            'sys.exit(' + str(exit_code) + ')\n'
        )
        url = '/cgi-bin/' + root.name + '/close_sleep.py'
        with concurrent.futures.ThreadPoolExecutor(max_workers=1) as pool:
            if method == 'POST':
                response = pool.submit(ctx.post, url, 'post-body', 6)
            else:
                response = pool.submit(ctx.get, url, 6)
            deadline = time.monotonic() + 3
            while not marker.exists():
                if time.monotonic() >= deadline:
                    raise AssertionError('CGI did not reach stdout closure')
                time.sleep(.01)
            # Allow the server to observe the hangup before testing responsiveness.
            time.sleep(.1)
            assert_status(ctx.get('/', timeout=.75), 200)
            if response.done():
                raise AssertionError('CGI was finalized before its child exited')
            raw = response.result(timeout=6)
            assert_status(raw, 500 if exit_code else 200)
            if not exit_code and get_body(raw) != 'complete:' + ('post-body' if method == 'POST' else ''):
                raise AssertionError('CGI output was truncated or changed')
            if Path('/proc').exists() and Path('/proc', marker.read_text()).exists():
                raise AssertionError('CGI child was not reaped before completion')


def test_closed_stdout_get(ctx):
    _closed_output_test(ctx)


def test_closed_stdout_post(ctx):
    _closed_output_test(ctx, method='POST')


def test_failure_after_stdout_closes(ctx):
    _closed_output_test(ctx, exit_code=1)


def test_buffered_output_after_exit(ctx):
    with tempfile.TemporaryDirectory(prefix='lifecycle-', dir=ctx.path('cgi-bin')) as directory:
        root = Path(directory)
        (root / 'large.py').write_text(
            'import sys\n'
            'sys.stdout.write("Content-Type: text/plain\\n\\n" + "x" * 131072)\n'
        )
        raw = ctx.get('/cgi-bin/' + root.name + '/large.py', timeout=5)
        assert_status(raw, 200)
        if get_body(raw) != 'x' * 131072:
            raise AssertionError('Buffered CGI output lost when the child exited')


def test_disconnected_post_child_is_reaped(ctx):
    with tempfile.TemporaryDirectory(prefix='lifecycle-', dir=ctx.path('cgi-bin')) as directory:
        root = Path(directory)
        marker = root / 'ready'
        (root / 'waiting.py').write_text(
            'import os, sys, time\n'
            'sys.stdin.read(int(os.environ.get("CONTENT_LENGTH", "0")))\n'
            'with open(' + repr(str(marker)) + ', "w") as f: f.write(str(os.getpid()))\n'
            'time.sleep(10)\n'
        )
        with socket.create_connection((ctx.host, ctx.port), timeout=3) as client:
            request = ('POST /cgi-bin/' + root.name + '/waiting.py HTTP/1.1\r\n'
                       'Host: localhost\r\nContent-Length: 4\r\n\r\nbody')
            client.sendall(request.encode())
            deadline = time.monotonic() + 3
            while not marker.exists():
                if time.monotonic() >= deadline:
                    raise AssertionError('CGI did not consume POST body')
                time.sleep(.01)
            child_pid = int(marker.read_text())
            # Force a disconnect detectable even while the client awaits CGI.
            client.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack('ii', 1, 0))
        try:
            deadline = time.monotonic() + 2
            while True:
                try:
                    os.kill(child_pid, 0)
                except ProcessLookupError:
                    break
                if time.monotonic() >= deadline:
                    raise AssertionError('Disconnected CGI child remained alive or a zombie')
                time.sleep(.02)
            assert_status(ctx.get('/', timeout=.75), 200)
        finally:
            try:
                os.kill(child_pid, signal.SIGKILL)
            except ProcessLookupError:
                pass


def register_tests():
    return [
        ('CGI stdout closure does not block GET', test_closed_stdout_get),
        ('CGI stdout closure does not block POST', test_closed_stdout_post),
        ('CGI failure after stdout closure returns 500', test_failure_after_stdout_closes),
        ('CGI output is drained completely after exit', test_buffered_output_after_exit),
        ('CGI child reaped after POST client disconnect', test_disconnected_post_child_is_reaped),
    ]
