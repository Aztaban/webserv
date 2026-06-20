import threading
from typing import List

from tests.lib.assertions import assert_http_response
from tests.lib.test_context import TestContext


def test_many_simultaneous_clients(ctx: TestContext) -> None:
    """
    Open many client connections at roughly the same time.

    This tests whether the poll loop remains stable under concurrent clients.
    """
    failures: List[str] = []
    client_count: int = 100

    def worker(index: int) -> None:
        """Single client worker used by the stress test."""
        try:
            response: str = ctx.get("/", timeout=3)
            assert_http_response(response)
        except Exception as error:
            failures.append("client {} failed: {}".format(index, error))

    threads: List[threading.Thread] = []

    for i in range(client_count):
        thread: threading.Thread = threading.Thread(target=worker, args=(i,))
        threads.append(thread)
        thread.start()

    for thread in threads:
        thread.join()

    if failures:
        raise AssertionError(failures)


def register_tests():
    return [
        ("many simultaneous clients", test_many_simultaneous_clients),
    ]
