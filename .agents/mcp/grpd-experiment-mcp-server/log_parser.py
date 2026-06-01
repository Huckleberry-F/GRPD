import os
import re
from typing import Any


STEP_RE = re.compile(r"\b(?:step|iter(?:ation)?)\s*[=:]?\s*(\d+)", re.IGNORECASE)
RESIDUAL_RE = re.compile(r"\b(?:residual|res|norm|error)\s*[=:]?\s*([-+]?\d+(?:\.\d+)?(?:[eE][-+]?\d+)?)", re.IGNORECASE)
CONVERGED_RE = re.compile(r"\b(converged|convergence achieved|solve completed)\b", re.IGNORECASE)
FAILED_RE = re.compile(r"\b(failed|diverged|nan|error|exception)\b", re.IGNORECASE)


def parse_grpd_log(log_file: str) -> dict[str, Any]:
    if not os.path.exists(log_file):
        raise FileNotFoundError(f"GRPD log file not found: {log_file}")

    num_steps = None
    final_residual = None
    residual_series = []
    converged = None
    status = "unknown"

    with open(log_file, "r", encoding="utf-8", errors="ignore") as file:
        for line in file:
            step_match = STEP_RE.search(line)
            residual_match = RESIDUAL_RE.search(line)
            if step_match:
                num_steps = int(step_match.group(1))
            if residual_match:
                value = float(residual_match.group(1))
                final_residual = value
                residual_series.append({"step": num_steps, "value": value})
            if CONVERGED_RE.search(line):
                converged = True
                status = "completed"
            if FAILED_RE.search(line):
                converged = False
                status = "failed"

    return {
        "success": True,
        "log_file": log_file,
        "status": status,
        "converged": converged,
        "num_steps": num_steps,
        "final_residual": final_residual,
        "residual_series": residual_series[-200:],
    }
