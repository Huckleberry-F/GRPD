#!/usr/bin/env python3
"""List GRPD static registration macro usages."""

from __future__ import annotations

import argparse
import json
import re
from collections import Counter
from pathlib import Path
import sys


REGISTER_RE = re.compile(r"\b(REGISTER_[A-Z_]+)\s*\((.*)\)")


def iter_source_files(root: Path, include_headers: bool):
    for base in ("PDCommon", "Src"):
        path = root / base
        if path.exists():
            yield from path.rglob("*.cpp")
            if include_headers:
                yield from path.rglob("*.h")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("root", nargs="?", default=".", help="GRPD repository root")
    parser.add_argument("--json", action="store_true", help="emit JSON")
    parser.add_argument("--include-headers", action="store_true", help="also scan headers for macro definitions/examples")
    args = parser.parse_args()
    root = Path(args.root).resolve()

    items = []
    counts: Counter[str] = Counter()
    for file in iter_source_files(root, args.include_headers):
        for line_no, line in enumerate(file.read_text(encoding="utf-8", errors="ignore").splitlines(), 1):
            match = REGISTER_RE.search(line)
            if not match:
                continue
            macro, raw_args = match.groups()
            counts[macro] += 1
            items.append({
                "file": file.relative_to(root).as_posix(),
                "line": line_no,
                "macro": macro,
                "args": raw_args.strip(),
            })

    if args.json:
        print(json.dumps({"summary": counts, "items": items}, ensure_ascii=False, indent=2))
    else:
        for item in items:
            print(f"{item['file']}:{item['line']} {item['macro']}({item['args']})")
        print("[SUMMARY] " + ", ".join(f"{k}={v}" for k, v in sorted(counts.items())))
    return 0


if __name__ == "__main__":
    sys.exit(main())
