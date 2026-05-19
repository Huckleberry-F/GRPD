"""
result_parser.py - Lightweight ANSYS result parsing helpers.

The MCP server exposes these helpers directly, and compare_and_export.py uses
parse_prvar_output() as the common table reader for ANSYS text exports.
"""

from __future__ import annotations

import glob
import os
import re
from pathlib import Path
from typing import Any


class ResultParser:
    """Parse ANSYS text outputs into small JSON-serializable dictionaries."""

    MAX_TEXT_CHARS = 200_000

    @staticmethod
    def _read_text(path: str, max_chars: int | None = None) -> str:
        limit = max_chars or ResultParser.MAX_TEXT_CHARS
        with open(path, "r", encoding="utf-8", errors="replace") as file:
            return file.read(limit)

    @staticmethod
    def _numeric_rows(content: str) -> list[list[float]]:
        rows: list[list[float]] = []
        number_pattern = re.compile(
            r"^[\s,;]*[-+]?(?:\d+(?:\.\d*)?|\.\d+)(?:[Ee][-+]?\d+)?"
        )

        for line in content.splitlines():
            stripped = line.strip()
            if not stripped or not number_pattern.match(stripped):
                continue

            parts = re.split(r"[\s,;]+", stripped)
            values: list[float] = []
            for part in parts:
                if not part:
                    continue
                try:
                    values.append(float(part))
                except ValueError:
                    values = []
                    break

            if values:
                rows.append(values)

        if not rows:
            return []

        width = max(len(row) for row in rows)
        return [row for row in rows if len(row) == width]

    @staticmethod
    def parse_out_file(out_file: str) -> dict[str, Any]:
        """Parse an ANSYS .out log for coarse solve status and diagnostics."""
        if not os.path.isfile(out_file):
            return {"success": False, "message": f"File not found: {out_file}"}

        content = ResultParser._read_text(out_file)
        errors = re.findall(r"\*\*\*\s*ERROR\s*\*\*\*", content, re.IGNORECASE)
        warnings = re.findall(r"\*\*\*\s*WARNING\s*\*\*\*", content, re.IGNORECASE)
        substeps = re.findall(r"SUBSTEP\s+(\d+)\s+COMPLETED", content, re.IGNORECASE)
        equils = re.findall(r"EQUIL\s+ITER\s+(\d+)\s+COMPLETED", content, re.IGNORECASE)
        elapsed = re.findall(
            r"ELAPSED\s+CP\s+TIME\s*[\(SEC\)]*\s*=\s*([\d.]+)",
            content,
            re.IGNORECASE,
        )

        result: dict[str, Any] = {
            "success": len(errors) == 0,
            "file": os.path.abspath(out_file),
            "errors": len(errors),
            "warnings": len(warnings),
            "completed_substeps": len(substeps),
            "total_equilibrium_iterations": len(equils),
        }

        if substeps:
            result["last_substep"] = int(substeps[-1])
        if elapsed:
            result["ansys_cp_time_sec"] = float(elapsed[-1])

        result["tail"] = content[-4000:]
        return result

    @staticmethod
    def parse_prvar_output(txt_file: str) -> dict[str, Any]:
        """Parse PRVAR/PRNSOL-style numeric text output."""
        if not os.path.isfile(txt_file):
            return {"success": False, "message": f"File not found: {txt_file}"}

        content = ResultParser._read_text(txt_file)
        rows = ResultParser._numeric_rows(content)
        if not rows:
            return {
                "success": False,
                "file": os.path.abspath(txt_file),
                "message": "No numeric table rows found.",
                "raw_preview": content[:4000],
            }

        n_cols = len(rows[0])
        columns = [f"col{i}" for i in range(n_cols)]
        stats: dict[str, dict[str, float]] = {}
        for idx, name in enumerate(columns):
            values = [row[idx] for row in rows]
            stats[name] = {"min": min(values), "max": max(values)}

        return {
            "success": True,
            "file": os.path.abspath(txt_file),
            "columns": columns,
            "num_rows": len(rows),
            "num_cols": n_cols,
            "stats": stats,
            "preview_rows": rows[:5],
            "data": rows,
        }

    @staticmethod
    def list_result_files(work_dir: str) -> dict[str, Any]:
        """List common ANSYS result and log files in a work directory."""
        if not os.path.isdir(work_dir):
            return {"success": False, "message": f"Directory not found: {work_dir}"}

        patterns = [
            "*.rst",
            "*.rth",
            "*.rnnn",
            "*.out",
            "*.err",
            "*.txt",
            "*.csv",
            "*.db",
            "*.log",
        ]

        files: list[dict[str, Any]] = []
        for pattern in patterns:
            for path in glob.glob(os.path.join(work_dir, pattern)):
                item = Path(path)
                files.append(
                    {
                        "path": str(item.resolve()),
                        "name": item.name,
                        "suffix": item.suffix,
                        "size_bytes": item.stat().st_size,
                    }
                )

        files.sort(key=lambda item: item["name"].lower())
        return {
            "success": True,
            "work_dir": os.path.abspath(work_dir),
            "count": len(files),
            "files": files,
        }
