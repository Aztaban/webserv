from dataclasses import dataclass


@dataclass
class TestResult:
    group: str
    name: str
    passed: bool
    message: str
    duration: float
