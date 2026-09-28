"""Startup diagnostics may write only to regular-file destinations."""
import os
from pathlib import Path
import pty
import subprocess
import tempfile


def test_logging_destinations(ctx):
    with tempfile.TemporaryDirectory(prefix='webserv-logging-') as directory:
        root = Path(directory)
        command = [ctx.path('webserv'), str(root / 'missing.conf')]
        result = subprocess.run(command, capture_output=True, timeout=3)
        assert result.returncode != 0
        assert result.stdout == result.stderr == b'', 'logging wrote to an unpolled pipe'
        with (root / 'diagnostics.log').open('wb') as log:
            result = subprocess.run(command, stdout=log, stderr=log, timeout=3)
        assert result.returncode != 0
        assert b'ERROR' in (root / 'diagnostics.log').read_bytes()
        master, slave = pty.openpty()
        try:
            result = subprocess.run(command, stdout=slave, stderr=slave, timeout=3)
            assert result.returncode != 0
            os.set_blocking(master, False)
            try:
                output = os.read(master, 4096)
            except BlockingIOError:
                output = b''
            assert output == b'', 'logging wrote to an unpolled terminal'
        finally:
            os.close(master)
            os.close(slave)


def register_tests():
    return [('logging retains file diagnostics and avoids pipes and terminals', test_logging_destinations)]
