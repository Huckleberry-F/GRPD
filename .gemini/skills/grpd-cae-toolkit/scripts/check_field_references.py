#!/usr/bin/env python3
"""Compare FieldManager string references with createField registrations."""

from __future__ import annotations

import argparse
import re
from pathlib import Path
import sys


CREATE_FIELD_RE = re.compile(r'createField\s*\(\s*"[^"]+"\s*,\s*"([^"]+)"')
TYPED_FIELD_RE = re.compile(r'TypedField\s*<[^>]+>\s*>*\s*\(\s*"([^"]+)"')
REF_PATTERNS = [
    re.compile(r'getFieldAs\s*<[^>]+>\s*\(\s*"([^"]+)"'),
    re.compile(r'getField\s*\(\s*"([^"]+)"'),
    re.compile(r'hasField\s*\(\s*"([^"]+)"'),
    re.compile(r'registerSwapPair\s*\(\s*"([^"]+)"\s*,\s*"([^"]+)"'),
    re.compile(r'add(?:Scalar|Vector|Tensor|Int)Field\s*\(\s*"([^"]+)"'),
]

KNOWN_RUNTIME_FIELDS = {
    "ID",
    "PartID",
    "MatID",
    "Coords",
    "Volume",
    "IsSurface",
    "ActiveStatus",
}


def source_files(root: Path):
    for base in ("PDCommon", "Src"):
        path = root / base
        if path.exists():
            yield from path.rglob("*.cpp")
            yield from path.rglob("*.h")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("root", nargs="?", default=".", help="GRPD repository root")
    args = parser.parse_args()
    root = Path(args.root).resolve()

    registered = set(KNOWN_RUNTIME_FIELDS)
    references: dict[str, list[str]] = {}

    for file in source_files(root):
        rel = file.relative_to(root).as_posix()
        text = file.read_text(encoding="utf-8", errors="ignore")
        for name in CREATE_FIELD_RE.findall(text):
            registered.add(name)
        for name in TYPED_FIELD_RE.findall(text):
            registered.add(name)
        for pattern in REF_PATTERNS:
            for match in pattern.finditer(text):
                for name in match.groups():
                    references.setdefault(name, []).append(rel)

    missing = sorted(name for name in references if name not in registered)
    if missing:
        for name in missing:
            places = ", ".join(sorted(set(references[name]))[:5])
            print(f"[MISSING_FIELD_REF] '{name}' referenced but no createField registration was found. Seen in: {places}")
        print("[WARN] This check is string-based; verify fields generated indirectly before editing.")
        return 1

    print(f"[OK] {len(references)} referenced field names are covered by detected registrations or known runtime fields.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
