#!/usr/bin/env python3
"""Lightweight PD.yaml sanity checks without requiring external packages."""

from __future__ import annotations

import argparse
import re
from pathlib import Path
import sys


TOP_LEVEL_REQUIRED = ["Solver"]
TOP_LEVEL_RECOMMENDED = ["System", "Materials", "LoadSteps"]


def top_level_keys(text: str) -> set[str]:
    keys = set()
    for line in text.splitlines():
        if line.startswith((" ", "\t", "#")):
            continue
        match = re.match(r"^([A-Za-z_][A-Za-z0-9_]*)\s*:", line)
        if match:
            keys.add(match.group(1))
    return keys


def scalar_after(text: str, key: str) -> str | None:
    match = re.search(rf"\b{re.escape(key)}\s*:\s*[\"']?([^\"'\n#]+)", text)
    return match.group(1).strip() if match else None


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("yaml", help="PD.yaml path")
    args = parser.parse_args()
    path = Path(args.yaml).resolve()
    text = path.read_text(encoding="utf-8", errors="ignore")
    keys = top_level_keys(text)
    failed = False

    for key in TOP_LEVEL_REQUIRED:
        if key not in keys:
            print(f"[ERROR] Missing required top-level block: {key}")
            failed = True
    for key in TOP_LEVEL_RECOMMENDED:
        if key not in keys and not (key == "Materials" and "Material" in keys):
            print(f"[WARN] Recommended top-level block not found: {key}")

    engine = scalar_after(text, "Engine")
    solver_type = scalar_after(text, "Type")
    integrator = scalar_after(text, "TimeIntegrator")
    if engine and engine != "PD":
        print(f"[WARN] Solver.Engine is '{engine}', expected 'PD' for current PDEngine.")
    if not solver_type:
        print("[WARN] Could not find Solver.Type; PhysicsFields auto-registration may not trigger.")
    if not integrator:
        print("[WARN] Could not find TimeIntegrator; check InitSolverComponents expectations.")

    for numeric_key in ("Horizon", "TimeStep_dt", "MassScaleFactor"):
        value = scalar_after(text, numeric_key)
        if value:
            try:
                if float(value) <= 0.0:
                    print(f"[ERROR] {numeric_key} must be positive, got {value}")
                    failed = True
            except ValueError:
                print(f"[WARN] {numeric_key} is not a plain numeric scalar: {value}")

    if failed:
        return 1
    print(f"[OK] Basic YAML sanity checks completed for {path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
