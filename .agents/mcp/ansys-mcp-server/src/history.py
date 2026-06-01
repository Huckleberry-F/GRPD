"""SQLite history helpers for ANSYS MCP validation runs."""

from __future__ import annotations

import sqlite3


def save_ansys_validation_to_db(
    db_path: str,
    run_res: dict,
    yaml_file: str,
    substep: int,
    job_name: str,
) -> None:
    """Persist a compact ANSYS validation run record."""
    conn = sqlite3.connect(db_path)
    try:
        cursor = conn.cursor()
        cursor.execute(
            """
            CREATE TABLE IF NOT EXISTS ansys_smoke_test_validation (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
                yaml_file TEXT,
                substep INTEGER,
                job_name TEXT,
                elapsed_seconds REAL,
                passed INTEGER,
                work_dir TEXT,
                mac_file TEXT,
                ansys_txt_file TEXT,
                db_file TEXT,
                error_log_tail TEXT
            )
            """
        )

        passed = 1 if run_res.get("success", False) else 0
        cursor.execute(
            """
            INSERT INTO ansys_smoke_test_validation
            (yaml_file, substep, job_name, elapsed_seconds, passed, work_dir,
             mac_file, ansys_txt_file, db_file, error_log_tail)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            """,
            (
                yaml_file,
                substep,
                job_name,
                run_res.get("elapsed_seconds", 0.0),
                passed,
                run_res.get("work_dir", ""),
                run_res.get("mac_file", ""),
                run_res.get("ansys_txt_file", ""),
                run_res.get("db_file", ""),
                run_res.get("error_log_tail", ""),
            ),
        )
        conn.commit()
    finally:
        conn.close()
