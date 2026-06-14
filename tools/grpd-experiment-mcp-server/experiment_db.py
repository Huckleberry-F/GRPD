import json
import os
import sqlite3
import time
import uuid
from typing import Any, Iterable


SCHEMA_VERSION = 1


def default_db_path() -> str:
    return os.path.join(os.path.dirname(__file__), "grpd_experiments.sqlite")


def connect(db_path: str = "") -> sqlite3.Connection:
    path = os.path.abspath(db_path or default_db_path())
    os.makedirs(os.path.dirname(path), exist_ok=True)
    conn = sqlite3.connect(path)
    conn.row_factory = sqlite3.Row
    ensure_schema(conn)
    return conn


def ensure_schema(conn: sqlite3.Connection) -> None:
    conn.executescript(
        """
        PRAGMA journal_mode=WAL;
        CREATE TABLE IF NOT EXISTS schema_info (
            version INTEGER NOT NULL
        );
        CREATE TABLE IF NOT EXISTS runs (
            run_id TEXT PRIMARY KEY,
            experiment_name TEXT NOT NULL,
            case_name TEXT NOT NULL,
            case_dir TEXT,
            git_commit TEXT,
            solver_type TEXT,
            kernel_type TEXT,
            material_type TEXT,
            status TEXT NOT NULL,
            converged INTEGER,
            start_time REAL NOT NULL,
            end_time REAL,
            elapsed_seconds REAL,
            num_steps INTEGER,
            final_residual REAL,
            max_error_uy_percent REAL,
            max_error_seqv_percent REAL,
            parameters_json TEXT NOT NULL,
            notes TEXT
        );
        CREATE TABLE IF NOT EXISTS metrics (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            run_id TEXT NOT NULL,
            name TEXT NOT NULL,
            value REAL,
            unit TEXT,
            step INTEGER,
            source TEXT,
            created_at REAL NOT NULL,
            FOREIGN KEY(run_id) REFERENCES runs(run_id) ON DELETE CASCADE
        );
        CREATE TABLE IF NOT EXISTS artifacts (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            run_id TEXT NOT NULL,
            kind TEXT NOT NULL,
            path TEXT NOT NULL,
            description TEXT,
            created_at REAL NOT NULL,
            FOREIGN KEY(run_id) REFERENCES runs(run_id) ON DELETE CASCADE
        );
        CREATE INDEX IF NOT EXISTS idx_runs_experiment ON runs(experiment_name);
        CREATE INDEX IF NOT EXISTS idx_runs_case ON runs(case_name);
        CREATE INDEX IF NOT EXISTS idx_runs_status ON runs(status);
        CREATE INDEX IF NOT EXISTS idx_metrics_run_name ON metrics(run_id, name);
        CREATE INDEX IF NOT EXISTS idx_artifacts_run_kind ON artifacts(run_id, kind);
        """
    )
    count = conn.execute("SELECT COUNT(*) AS n FROM schema_info").fetchone()["n"]
    if count == 0:
        conn.execute("INSERT INTO schema_info(version) VALUES (?)", (SCHEMA_VERSION,))
    conn.commit()


def _json_dumps(data: dict[str, Any]) -> str:
    return json.dumps(data or {}, ensure_ascii=False, sort_keys=True)


def _row_to_dict(row: sqlite3.Row) -> dict[str, Any]:
    data = dict(row)
    if "parameters_json" in data:
        data["parameters"] = json.loads(data.pop("parameters_json") or "{}")
    if "converged" in data and data["converged"] is not None:
        data["converged"] = bool(data["converged"])
    return data


def create_run(
    *,
    experiment_name: str,
    case_name: str,
    case_dir: str = "",
    parameters: dict[str, Any] | None = None,
    git_commit: str = "",
    solver_type: str = "",
    kernel_type: str = "",
    material_type: str = "",
    status: str = "created",
    notes: str = "",
    db_path: str = "",
) -> dict[str, Any]:
    run_id = str(uuid.uuid4())
    now = time.time()
    with connect(db_path) as conn:
        conn.execute(
            """
            INSERT INTO runs (
                run_id, experiment_name, case_name, case_dir, git_commit,
                solver_type, kernel_type, material_type, status, converged,
                start_time, parameters_json, notes
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, NULL, ?, ?, ?)
            """,
            (
                run_id,
                experiment_name,
                case_name,
                case_dir,
                git_commit,
                solver_type,
                kernel_type,
                material_type,
                status,
                now,
                _json_dumps(parameters or {}),
                notes,
            ),
        )
    return get_run(run_id, db_path=db_path)


def update_run(run_id: str, db_path: str = "", **fields: Any) -> dict[str, Any]:
    allowed = {
        "status",
        "converged",
        "end_time",
        "elapsed_seconds",
        "num_steps",
        "final_residual",
        "max_error_uy_percent",
        "max_error_seqv_percent",
        "notes",
        "git_commit",
        "solver_type",
        "kernel_type",
        "material_type",
        "case_dir",
    }
    updates = {k: v for k, v in fields.items() if k in allowed and v is not None}
    if "converged" in updates:
        updates["converged"] = int(bool(updates["converged"]))
    if not updates:
        return get_run(run_id, db_path=db_path)

    columns = ", ".join(f"{key}=?" for key in updates)
    values = list(updates.values()) + [run_id]
    with connect(db_path) as conn:
        conn.execute(f"UPDATE runs SET {columns} WHERE run_id=?", values)
    return get_run(run_id, db_path=db_path)


def finish_run(
    run_id: str,
    status: str,
    converged: bool | None = None,
    elapsed_seconds: float | None = None,
    num_steps: int | None = None,
    final_residual: float | None = None,
    max_error_uy_percent: float | None = None,
    max_error_seqv_percent: float | None = None,
    notes: str = "",
    db_path: str = "",
) -> dict[str, Any]:
    run = get_run(run_id, db_path=db_path)
    end_time = time.time()
    if elapsed_seconds is None:
        elapsed_seconds = end_time - float(run["start_time"])
    return update_run(
        run_id,
        db_path=db_path,
        status=status,
        converged=converged,
        end_time=end_time,
        elapsed_seconds=elapsed_seconds,
        num_steps=num_steps,
        final_residual=final_residual,
        max_error_uy_percent=max_error_uy_percent,
        max_error_seqv_percent=max_error_seqv_percent,
        notes=notes,
    )


def record_metric(
    run_id: str,
    name: str,
    value: float,
    unit: str = "",
    step: int | None = None,
    source: str = "",
    db_path: str = "",
) -> dict[str, Any]:
    now = time.time()
    with connect(db_path) as conn:
        cur = conn.execute(
            """
            INSERT INTO metrics(run_id, name, value, unit, step, source, created_at)
            VALUES (?, ?, ?, ?, ?, ?, ?)
            """,
            (run_id, name, value, unit, step, source, now),
        )
        metric_id = cur.lastrowid
    return {"success": True, "id": metric_id, "run_id": run_id}


def record_artifact(
    run_id: str,
    kind: str,
    path: str,
    description: str = "",
    db_path: str = "",
) -> dict[str, Any]:
    now = time.time()
    with connect(db_path) as conn:
        cur = conn.execute(
            """
            INSERT INTO artifacts(run_id, kind, path, description, created_at)
            VALUES (?, ?, ?, ?, ?)
            """,
            (run_id, kind, path, description, now),
        )
        artifact_id = cur.lastrowid
    return {"success": True, "id": artifact_id, "run_id": run_id}


def get_run(run_id: str, db_path: str = "") -> dict[str, Any]:
    with connect(db_path) as conn:
        row = conn.execute("SELECT * FROM runs WHERE run_id=?", (run_id,)).fetchone()
        if row is None:
            raise KeyError(f"Run not found: {run_id}")
        run = _row_to_dict(row)
        run["metrics"] = [dict(r) for r in conn.execute("SELECT * FROM metrics WHERE run_id=? ORDER BY id", (run_id,))]
        run["artifacts"] = [dict(r) for r in conn.execute("SELECT * FROM artifacts WHERE run_id=? ORDER BY id", (run_id,))]
        return run


def query_runs(
    experiment_name: str = "",
    case_name: str = "",
    status: str = "",
    limit: int = 20,
    db_path: str = "",
) -> list[dict[str, Any]]:
    clauses = []
    values: list[Any] = []
    if experiment_name:
        clauses.append("experiment_name = ?")
        values.append(experiment_name)
    if case_name:
        clauses.append("case_name = ?")
        values.append(case_name)
    if status:
        clauses.append("status = ?")
        values.append(status)
    where = "WHERE " + " AND ".join(clauses) if clauses else ""
    sql = f"SELECT * FROM runs {where} ORDER BY start_time DESC LIMIT ?"
    values.append(max(1, min(int(limit), 500)))
    with connect(db_path) as conn:
        return [_row_to_dict(row) for row in conn.execute(sql, values)]


def compare_parameter_sweep(
    experiment_name: str,
    parameter_name: str,
    metric_name: str = "final_residual",
    case_name: str = "",
    db_path: str = "",
) -> dict[str, Any]:
    runs = query_runs(experiment_name=experiment_name, case_name=case_name, limit=500, db_path=db_path)
    points = []
    for run in runs:
        parameters = run.get("parameters", {})
        if parameter_name not in parameters:
            continue
        metric_value = run.get(metric_name)
        if metric_value is None:
            metric_value = _latest_metric_value(run["run_id"], metric_name, db_path=db_path)
        if metric_value is None:
            continue
        points.append(
            {
                "run_id": run["run_id"],
                "case_name": run["case_name"],
                "status": run["status"],
                "parameter": parameters[parameter_name],
                "metric": metric_value,
            }
        )

    numeric_points = [p for p in points if isinstance(p["parameter"], (int, float))]
    numeric_points.sort(key=lambda p: p["parameter"])
    best = min(points, key=lambda p: p["metric"]) if points else None
    return {
        "success": True,
        "experiment_name": experiment_name,
        "parameter_name": parameter_name,
        "metric_name": metric_name,
        "count": len(points),
        "points": numeric_points if numeric_points else points,
        "best": best,
    }


def summarize_convergence(experiment_name: str = "", case_name: str = "", db_path: str = "") -> dict[str, Any]:
    runs = query_runs(experiment_name=experiment_name, case_name=case_name, limit=500, db_path=db_path)
    total = len(runs)
    converged = sum(1 for run in runs if run.get("converged") is True)
    failed = sum(1 for run in runs if run.get("status") in {"failed", "error"})
    residuals = [run["final_residual"] for run in runs if run.get("final_residual") is not None]
    elapsed = [run["elapsed_seconds"] for run in runs if run.get("elapsed_seconds") is not None]
    return {
        "success": True,
        "total_runs": total,
        "converged_runs": converged,
        "failed_runs": failed,
        "convergence_rate": (converged / total) if total else None,
        "min_final_residual": min(residuals) if residuals else None,
        "max_final_residual": max(residuals) if residuals else None,
        "avg_elapsed_seconds": (sum(elapsed) / len(elapsed)) if elapsed else None,
    }


def _latest_metric_value(run_id: str, metric_name: str, db_path: str = "") -> float | None:
    with connect(db_path) as conn:
        row = conn.execute(
            """
            SELECT value FROM metrics
            WHERE run_id=? AND name=? AND value IS NOT NULL
            ORDER BY created_at DESC, id DESC LIMIT 1
            """,
            (run_id, metric_name),
        ).fetchone()
    return None if row is None else row["value"]
