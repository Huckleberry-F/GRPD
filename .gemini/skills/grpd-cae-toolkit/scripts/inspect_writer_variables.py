#!/usr/bin/env python3
"""Inspect Writer.Variables entries in PD.yaml files and compare with known fields."""

from __future__ import annotations

import argparse
import re
from pathlib import Path
import sys


CREATE_FIELD_RE = re.compile(r'createField\s*\(\s*"[^"]+"\s*,\s*"([^"]+)"')
NAME_RE = re.compile(r"\bName\s*:\s*[\"']?([^\"'\]\},#]+)")
DIM_RE = re.compile(r"\bDimension\s*:\s*([0-9]+)")
KNOWN_FIELDS = {
    "ID",
    "PartID",
    "MatID",
    "Coords",
    "Volume",
    "IsSurface",
    "ActiveStatus",
}


def known_fields(root: Path) -> set[str]:
    fields = set(KNOWN_FIELDS)
    for base in ("PDCommon", "Src"):
        path = root / base
        if not path.exists():
            continue
        for file in list(path.rglob("*.cpp")) + list(path.rglob("*.h")):
            text = file.read_text(encoding="utf-8", errors="ignore")
            fields.update(CREATE_FIELD_RE.findall(text))
    return fields


def yaml_files(root: Path, explicit: list[str]) -> list[Path]:
    if explicit:
        return [Path(p).resolve() for p in explicit]
    examples = root / "Examples"
    return sorted(examples.rglob("PD.yaml")) if examples.exists() else []


def extract_writer_variables(path: Path) -> list[tuple[str, int | None]]:
    lines = path.read_text(encoding="utf-8", errors="ignore").splitlines()
    in_writer = False
    in_variables = False
    current_name: str | None = None
    current_dim: int | None = None
    result: list[tuple[str, int | None]] = []

    def flush():
        nonlocal current_name, current_dim
        if current_name:
            result.append((current_name.strip(), current_dim))
        current_name = None
        current_dim = None

    for line in lines:
        stripped = line.strip()
        if not stripped or stripped.startswith("#"):
            continue
        if re.match(r"^[A-Za-z_][A-Za-z0-9_]*\s*:", line):
            if in_writer and not line.startswith((" ", "\t")):
                flush()
                in_writer = False
                in_variables = False
            if stripped.startswith("Writer:"):
                in_writer = True
                in_variables = False
            continue
        if not in_writer:
            continue
        if stripped.startswith("Variables:"):
            in_variables = True
            continue
        if not in_variables:
            continue
        if stripped.startswith("-"):
            flush()
        name_match = NAME_RE.search(stripped)
        if name_match:
            current_name = name_match.group(1).strip()
        dim_match = DIM_RE.search(stripped)
        if dim_match:
            current_dim = int(dim_match.group(1))
    flush()
    return result


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("root", nargs="?", default=".", help="GRPD repository root")
    parser.add_argument("--yaml", action="append", default=[], help="specific PD.yaml file")
    args = parser.parse_args()
    root = Path(args.root).resolve()
    fields = known_fields(root)
    failed = False

    for yaml_path in yaml_files(root, args.yaml):
        variables = extract_writer_variables(yaml_path)
        if not variables:
            continue
        rel = yaml_path.relative_to(root).as_posix() if yaml_path.is_relative_to(root) else str(yaml_path)
        for name, dim in variables:
            if name == "Coords":
                print(f"[INFO] {rel}: Coords is used as point coordinates and is skipped as a normal variable.")
                continue
            if name not in fields:
                print(f"[UNKNOWN_WRITER_FIELD] {rel}: Writer variable '{name}' was not found in detected FieldManager registrations.")
                failed = True
            else:
                print(f"[OK] {rel}: Writer variable '{name}' dimension={dim if dim is not None else 'unspecified'}")

    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
