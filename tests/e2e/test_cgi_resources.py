"""Check CGI descriptor isolation and failure cleanup over real HTTP requests."""
import json
import os
from pathlib import Path
import tempfile
import time

from tests.lib.assertions import assert_status
from tests.lib.http_response import get_body


def test_cgi_does_not_inherit_server_descriptors(ctx):
    with tempfile.TemporaryDirectory(prefix='resources-', dir=ctx.path('cgi-bin')) as directory:
        root = Path(directory)
        (root / 'fds.py').write_text(
            'import os,json\n'
            'leaked=[]\n'
            'for fd in os.listdir("/proc/self/fd"):\n'
            '    if int(fd) <= 2: continue\n'
            '    try: target=os.readlink("/proc/self/fd/"+fd)\n'
            '    except OSError: continue\n'
            '    if target.startswith(("socket:", "pipe:")): leaked.append(target)\n'
            'print("Content-Type: application/json\\n")\n'
            'print(json.dumps(leaked))\n'
        )
        for _ in range(5):
            raw = ctx.get('/cgi-bin/' + root.name + '/fds.py', timeout=3)
            assert_status(raw, 200)
            leaked = json.loads(get_body(raw))
            if leaked:
                raise AssertionError('CGI inherited server descriptors: ' + repr(leaked))


def test_cgi_rejected_post_returns_500(ctx):
    with tempfile.TemporaryDirectory(prefix='resources-', dir=ctx.path('cgi-bin')) as directory:
        root = Path(directory)
        marker = root / 'child.pid'
        (root / 'reject.py').write_text(
            'import os,time\n'
            'with open(' + repr(str(marker)) + ', "w") as f: f.write(str(os.getpid()))\n'
            'os.close(0)\n'
            'time.sleep(.1)\n'
            'raise RuntimeError("POST rejected by CGI")\n'
        )
        raw = ctx.post('/cgi-bin/' + root.name + '/reject.py', 'x' * 60000, timeout=3)
        assert_status(raw, 500)
        child_pid = int(marker.read_text())
        deadline = time.monotonic() + 2
        while True:
            try:
                os.kill(child_pid, 0)
            except ProcessLookupError:
                break
            if time.monotonic() >= deadline:
                raise AssertionError('Failed CGI child remained alive or a zombie')
            time.sleep(.02)
        assert_status(ctx.get('/', timeout=1), 200)


def register_tests():
    return [
        ('CGI inherits no server sockets or extra pipes', test_cgi_does_not_inherit_server_descriptors),
        ('CGI rejected POST returns 500 and reaps child', test_cgi_rejected_post_returns_500),
    ]
