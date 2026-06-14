#!/usr/bin/env python3
"""Check whether module src/*.cpp files are listed in their CMakeLists.txt."""

from __future__ import annotations

import argparse
from pathlib import Path
import sys


MODULES = [
    "PDCommon/Material",
    "PDCommon/Kernel",
    "PDCommon/BC",
    "PDCommon/Contact",
    "PDCommon/Field",
    "PDCommon/Fracture",
    "PDCommon/Neighbor",
    "PDCommon/IO",
    "PDCommon/Model",
    "PDCommon/Core",
    "PDCommon/PostProcessing",
    "PDCommon/Utils",
    "Src/Integration",
    "Src/Engine",
    "Src/Engine/Solvers/PD",
]


def cmake_contains(text: str, rel: str) -> bool:
    return rel in text or rel.replace("/", "\\") in text


def check_module(root: Path, module_rel: str) -> list[str]:
    module = root / module_rel
    cmake = module / "CMakeLists.txt"
    src_dir = module / "src"
    if not module.exists() or not cmake.exists() or not src_dir.exists():
        return []

    text = cmake.read_text(encoding="utf-8", errors="ignore")
    messages: list[str] = []
    for cpp in sorted(src_dir.glob("*.cpp")):
        rel = cpp.relative_to(module).as_posix()
        if not cmake_contains(text, rel):
            messages.append(f"[MISSING_CPP] {module_rel}/{rel} not listed in {module_rel}/CMakeLists.txt")
    return messages


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("root", nargs="?", default=".", help="GRPD repository root")
    args = parser.parse_args()
    root = Path(args.root).resolve()

    all_messages: list[str] = []
    for module_rel in MODULES:
        all_messages.extend(check_module(root, module_rel))

    if all_messages:
        for msg in all_messages:
            print(msg)
        return 1

    print("[OK] All scanned src/*.cpp files are listed in module CMakeLists.txt files.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
