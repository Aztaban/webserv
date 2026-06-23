import html
from typing import List

from tests.lib.test_result import TestResult


def _build_summary(results: List[TestResult]) -> str:
    passed: int = sum(1 for result in results if result.passed)
    total: int = len(results)
    failed: int = total - passed

    return (
        "<div class=\"summary-card\">"
        "<div><strong>Total:</strong> {}</div>"
        "<div><strong>Passed:</strong> {}</div>"
        "<div><strong>Failed:</strong> {}</div>"
        "</div>"
    ).format(total, passed, failed)


def _build_rows(results: List[TestResult]) -> str:
    rows: List[str] = []

    for result in results:
        status: str = "PASS" if result.passed else "FAIL"
        css_class: str = "pass" if result.passed else "fail"

        rows.append(
            "<tr class=\"{css}\">"
            "<td>{status}</td>"
            "<td>{group}</td>"
            "<td>{name}</td>"
            "<td>{duration:.3f}s</td>"
            "<td>{message}</td>"
            "</tr>".format(
                css=css_class,
                status=status,
                group=html.escape(result.group),
                name=html.escape(result.name),
                duration=result.duration,
                message=html.escape(result.message),
            )
        )

    return "\n".join(rows)


def generate_html_report(
    results: List[TestResult],
    template_path: str,
    output_path: str,
) -> None:
    with open(template_path, "r") as file:
        template: str = file.read()

    output: str = template.replace("<!-- TEST_SUMMARY -->", _build_summary(results))
    output = output.replace("<!-- TEST_ROWS -->", _build_rows(results))

    with open(output_path, "w") as file:
        file.write(output)
