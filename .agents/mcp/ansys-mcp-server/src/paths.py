"""Path and configuration helpers for the ANSYS MCP server."""

from __future__ import annotations

import os
from pathlib import Path
from typing import Any

import yaml


SERVER_ROOT = Path(__file__).resolve().parents[1]


def service_root() -> str:
    """Return the absolute ANSYS MCP service root directory."""
    return str(SERVER_ROOT)


def service_path(*parts: str) -> str:
    """Return an absolute path below the ANSYS MCP service root."""
    return str(SERVER_ROOT.joinpath(*parts))


def get_next_work_dir(base_dir: str, prefix: str = "run_") -> str:
    """Return the next incrementing work directory path under ``base_dir``."""
    os.makedirs(base_dir, exist_ok=True)
    existing_folders = [
        item
        for item in os.listdir(base_dir)
        if os.path.isdir(os.path.join(base_dir, item))
    ]

    max_idx = 0
    for folder in existing_folders:
        if not folder.startswith(prefix):
            continue
        try:
            idx = int(folder[len(prefix):])
        except ValueError:
            continue
        max_idx = max(max_idx, idx)

    return os.path.join(base_dir, f"{prefix}{max_idx + 1:04d}")


def load_config(config_file: str = "") -> dict[str, Any]:
    """Load ``config.yaml`` from the service root unless a path is provided."""
    path = config_file or service_path("config.yaml")
    with open(path, "r", encoding="utf-8") as file:
        return yaml.safe_load(file) or {}


def default_work_dir_base() -> str:
    """Return the default work directory base for generated ANSYS runs."""
    return os.path.abspath(service_path("work_dir"))


def validation_db_file() -> str:
    """Return the ANSYS validation history database path."""
    return service_path("validation_history.db")


def allowed_directories_from_config(config: dict[str, Any]) -> list[str]:
    """Return configured allowed directories plus the service work directory."""
    allowed_dirs = [
        os.path.abspath(path)
        for path in config.get("allowed_directories", [])
    ]
    default_base = default_work_dir_base()
    if default_base not in allowed_dirs:
        allowed_dirs.append(default_base)
    return allowed_dirs
