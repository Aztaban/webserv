"""
Settings for the Python test runner.

These values must match the test webserv config in:

    tests/fixtures/test.conf

Do not parse the webserv config file here yet.
The webserv config grammar may still change during development.
"""

HOST: str = "127.0.0.1"
PORT: int = 8000
SECOND_PORT: int = 8001

CONFIG_PATH: str = "tests/fixtures/test.conf"

REPORT_TEMPLATE_PATH: str = "tests/report_template.html"
REPORT_OUTPUT_PATH: str = "tests/report.html"
SERVER_LOG_PATH: str = "tests/server_test.log"