from __future__ import annotations

import os
import re
from typing import Any


class ResultParser:
    """Parse ANSYS PRVAR/PRNSOL-style text exports for validation comparisons."""

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

            values: list[float] = []
            for part in re.split(r"[\s,;]+", stripped):
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
    def parse_prvar_output(txt_file: str) -> dict[str, Any]:
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
        return {
            "success": True,
            "file": os.path.abspath(txt_file),
            "columns": columns,
            "num_rows": len(rows),
            "num_cols": n_cols,
            "data": rows,
        }
