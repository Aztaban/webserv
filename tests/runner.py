#!/usr/bin/env python3

import importlib.util
import os
import subprocess
import sys
import time
from typing import Callable, List, Optional, Tuple

ROOT_DIR: str = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))

if ROOT_DIR not in sys.path:
    sys.path.insert(0, ROOT_DIR)

from tests.lib.html_report import generate_html_report
from tests.lib.http_client import can_connect
from tests.lib.test_context import TestContext
from tests.lib.test_result import TestResult


WEBSERV_PATH: str = os.path.join(ROOT_DIR, "webserv")
CONFIG_PATH: str = os.path.join(ROOT_DIR, "tests", "fixtures", "test.conf")
TEMPLATE_PATH: str = os.path.join(ROOT_DIR, "tests", "report_template.html")
REPORT_PATH: str = os.path.join(ROOT_DIR, "tests", "report.html")

# These values should match tests/fixtures/test.conf.
HOST: str = "127.0.0.1"
PORT: int = 8000

TEST_DIRS: List[str] = [
    "network",
    "e2e"
]

TestFunction = Callable[[TestContext], None]
RegisteredTest = Tuple[str, TestFunction]


class RunnerError(Exception):
    pass


def build_project() -> None:
    """Build webserv before executing tests."""
    result: subprocess.CompletedProcess = subprocess.run(["make"], cwd=ROOT_DIR)

    if result.returncode != 0:
        raise RunnerError("make failed")


def wait_for_server(timeout: float = 3.0) -> bool:
    """Wait until the listening socket becomes available."""
    start: float = time.time()

    while time.time() - start < timeout:
        if can_connect(HOST, PORT, timeout=0.2):
            return True
        time.sleep(0.05)

    return False


def stop_server(server: subprocess.Popen) -> None:
    """Stop webserv."""
    if server.poll() is not None:
        return

    server.terminate()

    try:
        server.wait(timeout=2)
    except subprocess.TimeoutExpired:
        server.kill()
        server.wait()


def start_server() -> subprocess.Popen:
    """Launch webserv using the test configuration."""
    if not os.path.exists(WEBSERV_PATH):
        raise RunnerError("webserv executable not found")

    if not os.path.exists(CONFIG_PATH):
        raise RunnerError("test config not found: {}".format(CONFIG_PATH))

    log_path: str = os.path.join(ROOT_DIR, "tests", "server_test.log")
    log_file = open(log_path, "w")
    
    server: subprocess.Popen = subprocess.Popen(
        [WEBSERV_PATH, CONFIG_PATH],
        cwd=ROOT_DIR,
        stdout=log_file,
        stderr=log_file,
    )

    if not wait_for_server():
        stop_server(server)
        raise RunnerError("server did not start or port {} is not open".format(PORT))

    return server


def load_module_from_path(path: str):
    """
    Import a Python test file dynamically.

    This allows adding new test files without
    modifying runner.py.
    """
    module_name: str = path.replace("/", "_").replace(".", "_")

    spec = importlib.util.spec_from_file_location(module_name, path)

    if spec is None or spec.loader is None:
        raise RunnerError("could not load test module: {}".format(path))

    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)

    return module


def discover_python_tests(selected_group: str) -> List[Tuple[str, str, TestFunction]]:
    """Discover all tests belonging to the selected group."""
    discovered: List[Tuple[str, str, TestFunction]] = []

    for group in TEST_DIRS:
        if selected_group != "all" and selected_group != group:
            continue

        group_dir: str = os.path.join(ROOT_DIR, "tests", group)

        if not os.path.isdir(group_dir):
            continue
        
        for filename in sorted(os.listdir(group_dir)):
            if not filename.startswith("test_") or not filename.endswith(".py"):
                continue

            path: str = os.path.join(group_dir, filename)
            module = load_module_from_path(path)

            if not hasattr(module, "register_tests"):
                continue

            registered_tests: List[RegisteredTest] = module.register_tests()

            for test_name, test_function in registered_tests:
                discovered.append((group, test_name, test_function))

    return discovered


def run_one_test(
    group: str,
    name: str,
    function: TestFunction,
    context: TestContext,
) -> TestResult:
    """Execute a single test and convert the result into a TestResult object."""
    start: float = time.time()

    try:
        function(context)
        duration: float = time.time() - start
        print("[PASS] {} / {}".format(group, name))
        return TestResult(group, name, True, "", duration)

    except Exception as error:
        duration = time.time() - start
        print("[FAIL] {} / {} -> {}".format(group, name, error))
        return TestResult(group, name, False, str(error), duration)


def print_usage() -> None:
    print("Usage:")
    print("  python3 tests/runner.py")
    print("  python3 tests/runner.py network")
    print("  python3 tests/runner.py e2e")


def main() -> int:
    selected_group: str = "all"

    if len(sys.argv) > 2:
        print_usage()
        return 1

    if len(sys.argv) == 2:
        selected_group = sys.argv[1]

    tests: List[Tuple[str, str, TestFunction]] = discover_python_tests(selected_group)

    if not tests:
        print("No tests found for group: {}".format(selected_group))
        return 1

    context = TestContext(
        root_dir=ROOT_DIR,
        host=HOST,
        port=PORT,
        config_path=CONFIG_PATH,
    )

    server: Optional[subprocess.Popen] = None
    results: List[TestResult] = []

    try:
        print("[INFO] Building project")
        build_project()

        print("[INFO] Starting server")
        server = start_server()

        print("[INFO] Running {} test(s)".format(len(tests)))

        for group, name, function in tests:
            result: TestResult = run_one_test(group, name, function, context)
            results.append(result)

        generate_html_report(results, TEMPLATE_PATH, REPORT_PATH)

        passed: int = sum(1 for result in results if result.passed)
        total: int = len(results)

        print()
        print("Result: {}/{} tests passed".format(passed, total))
        print("HTML report: {}".format(REPORT_PATH))

        if passed != total:
            return 1
        
        return 0

    except Exception as error:
        print("[FAIL] {}".format(error))
        return 1

    finally:
        if server is not None:
            stop_server(server)


if __name__ == "__main__":
    sys.exit(main())
