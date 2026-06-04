#!/usr/bin/env python3
"""Validate GRPD's Antigravity Superpowers overlay files.

This script checks only the lightweight overlay introduced for GRPD. It does
not validate or rewrite the upstream Superpowers skill set.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]

REQUIRED_FILES = [
    ".agents/skills/superCAE/SKILL.md",
    ".agents/skills/superCAE/skills/grpd-cae-router/SKILL.md",
    ".agents/skills/superpowers/skills/single-flow-task-execution/SKILL.md",
    ".agents/workflows/grpd-route.md",
    ".agents/workflows/execute-single-flow.md",
]

REQUIRED_SPECIALIST_SKILLS = [
    "cae-input-schema-validator",
    "cmake-build-doctor",
    "constitutive-math-reviewer",
    "grpd-cae-toolkit",
    "grpd-experiment-tracker",
    "grpd-smoke-tester",
    "material-model-implementer",
    "mesh-and-neighbor-reviewer",
    "numerical-test-generator",
    "openmp-kernel-optimizer",
    "physics-field-pipeline-auditor",
    "postprocess-exporter",
    "solver-loop-safety-reviewer",
]

LEGACY_PATTERNS = [
    r"\bTodoWrite\b",
    r"\bTask tool\b",
    r"Task\(",
    r"Dispatch .*subagent",
    r"superpowers:",
    r"artifacts/superpowers",
    r"brain/<conv-id>",
]


class Check:
    def __init__(self) -> None:
        self.passed = 0
        self.failed = 0

    def pass_(self, message: str) -> None:
        self.passed += 1
        print(f"[PASS] {message}")

    def fail(self, message: str) -> None:
        self.failed += 1
        print(f"[FAIL] {message}")

    def require(self, condition: bool, message: str) -> None:
        if condition:
            self.pass_(message)
        else:
            self.fail(message)


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def has_frontmatter(text: str, expected_name: str) -> bool:
    match = re.match(r"^---\n(?P<body>.*?)\n---\n", text, re.DOTALL)
    if not match:
        return False
    body = match.group("body")
    return (
        re.search(rf"^name:\s*{re.escape(expected_name)}\s*$", body, re.MULTILINE)
        and re.search(r"^description:\s*.+", body, re.MULTILINE)
    ) is not None


def main() -> int:
    check = Check()

    for relative in REQUIRED_FILES:
        check.require((ROOT / relative).is_file(), f"required file exists: {relative}")

    router = ROOT / ".agents/skills/superCAE/skills/grpd-cae-router/SKILL.md"
    single_flow = ROOT / ".agents/skills/superpowers/skills/single-flow-task-execution/SKILL.md"

    if router.is_file():
        router_text = read_text(router)
        check.require(has_frontmatter(router_text, "grpd-cae-router"), "grpd-cae-router frontmatter is valid")

        for skill in REQUIRED_SPECIALIST_SKILLS:
            skill_path = ROOT / ".agents/skills/superCAE/skills" / skill / "SKILL.md"
            check.require(skill_path.is_file(), f"specialist skill exists: {skill}")
            check.require(skill in router_text, f"router mentions specialist skill: {skill}")

        required_router_fields = [
            "Touched Pipeline Layers",
            "Domain Skills",
            "State Risk",
            "SoA Risk",
            "Parallel Risk",
            "Validation",
        ]
        for field in required_router_fields:
            check.require(field in router_text, f"router requires plan/review field: {field}")

    if single_flow.is_file():
        single_flow_text = read_text(single_flow)
        check.require(has_frontmatter(single_flow_text, "single-flow-task-execution"), "single-flow frontmatter is valid")
        check.require("docs/superpowers/task.md" in single_flow_text, "single-flow uses visible runtime task table")
        check.require("grpd-cae-router" in single_flow_text, "single-flow requires GRPD CAE router")
        check.require("verification-before-completion" in single_flow_text, "single-flow requires completion verification")

        for pattern in LEGACY_PATTERNS:
            check.require(
                re.search(pattern, single_flow_text) is None,
                f"single-flow overlay has no unsupported legacy pattern: {pattern}",
            )

    gitignore = ROOT / ".gitignore"
    if gitignore.is_file():
        gitignore_text = read_text(gitignore)
        check.require("docs/superpowers/task.md" in gitignore_text, "runtime task table is ignored by git")

    print("")
    print(f"Passed: {check.passed}")
    print(f"Failed: {check.failed}")
    return 1 if check.failed else 0


if __name__ == "__main__":
    sys.exit(main())
