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

from tests import test_settings
from tests.lib.html_report import generate_html_report
from tests.lib.http_client import can_connect
from tests.lib.test_context import TestContext
from tests.lib.test_result import TestResult


WEBSERV_PATH: str = os.path.join(ROOT_DIR, "webserv")

CONFIG_PATH: str = os.path.join(ROOT_DIR, test_settings.CONFIG_PATH)
TEMPLATE_PATH: str = os.path.join(ROOT_DIR, test_settings.REPORT_TEMPLATE_PATH)
REPORT_PATH: str = os.path.join(ROOT_DIR, test_settings.REPORT_OUTPUT_PATH)
SERVER_LOG_PATH: str = os.path.join(ROOT_DIR, test_settings.SERVER_LOG_PATH)

HOST: str = test_settings.HOST
PORT: int = test_settings.PORT
SECOND_PORT: int = test_settings.SECOND_PORT

TEST_DIRS: List[str] = [
    "network",
    "e2e"
]

CPP_UNIT_TESTS: List[Tuple[str, List[str]]] = [
    (
        "parser_tests",
        [
            "tests/unit/parser_tests.cpp",
            "src/http/HttpRequestParser.cpp"
        ],
    ),
    (
        "response_builder_tests",
        [
            "tests/unit/response_builder_tests.cpp",
            "src/http/HttpResponseBuilder.cpp",
        ],
    )
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

    log_file = open(SERVER_LOG_PATH, "w")
    
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


def _run_command(command: List[str], cwd: str) -> subprocess.CompletedProcess:
    """Run a command and keep stdout/stderr for test failure messages."""
    return subprocess.run(
        command,
        cwd=cwd,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        universal_newlines=True,
    )


def _command_output(result: subprocess.CompletedProcess) -> str:
    output: str = (result.stdout or "") + (result.stderr or "")
    return output.strip()


def run_cpp_unit_test(name: str, sources: List[str]) -> TestResult:
    """Compile and run one C++ unit test binary."""
    start: float = time.time()
    bin_dir: str = os.path.join(ROOT_DIR, "tests", "bin")
    binary_path: str = os.path.join(bin_dir, name)

    if not os.path.isdir(bin_dir):
        os.makedirs(bin_dir)

    compile_command: List[str] = [
        "c++",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-std=c++98",
        "-Iinclude",
    ]
    compile_command.extend(sources)
    compile_command.extend(["-o", binary_path])

    compile_result: subprocess.CompletedProcess = _run_command(compile_command, ROOT_DIR)

    if compile_result.returncode != 0:
        duration: float = time.time() - start
        message: str = "compile failed\n{}".format(_command_output(compile_result))
        print("[FAIL] unit / {} -> compile failed".format(name))
        return TestResult("unit", name, False, message, duration)

    run_result: subprocess.CompletedProcess = _run_command([binary_path], ROOT_DIR)
    duration: float = time.time() - start

    if run_result.returncode != 0:
        message = "test failed\n{}".format(_command_output(run_result))
        print("[FAIL] unit / {} -> test failed".format(name))
        return TestResult("unit", name, False, message, duration)

    print("[PASS] unit / {}".format(name))
    return TestResult("unit", name, True, "", duration)


def run_cpp_unit_tests() -> List[TestResult]:
    results: List[TestResult] = []

    for name, sources in CPP_UNIT_TESTS:
        results.append(run_cpp_unit_test(name, sources))

    return results


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
    print("  python3 tests/runner.py unit")


def main() -> int:
    selected_group: str = "all"

    if len(sys.argv) > 2:
        print_usage()
        return 1

    if len(sys.argv) == 2:
        selected_group = sys.argv[1]

    if selected_group not in ["all", "network", "e2e", "unit"]:
        print_usage()
        return 1
    
    tests: List[Tuple[str, str, TestFunction]] = discover_python_tests(selected_group)
    run_units: bool = (selected_group == "all" or selected_group == "unit")

    if not tests:
        print("No tests found for group: {}".format(selected_group))
        return 1

    context = TestContext(
        root_dir=ROOT_DIR,
        host=HOST,
        port=PORT,
        second_port=SECOND_PORT,
        config_path=CONFIG_PATH,
    )

    server: Optional[subprocess.Popen] = None
    results: List[TestResult] = []

    try:
        if tests:
            print("[INFO] Building project")
            build_project()

            print("[INFO] Starting server")
            server = start_server()

            print("[INFO] Running {} test(s)".format(len(tests)))

            for group, name, function in tests:
                result: TestResult = run_one_test(group, name, function, context)
                results.append(result)

        if run_units:
            print("[INFO] Running C++ unit tests")
            unit_results: List[TestResult] = run_cpp_unit_tests()

            for result in unit_results:
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
