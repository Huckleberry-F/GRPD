"""
server.py - GRPD experiment tracking MCP server.

使用 SQLite 记录 GRPD 算例参数、收敛状态、误差指标和结果文件路径。
"""

from __future__ import annotations

from typing import Any

from mcp.server.fastmcp import FastMCP

from experiment_db import (
    compare_parameter_sweep,
    create_run,
    finish_run,
    get_run,
    query_runs,
    record_artifact,
    record_metric,
    summarize_convergence,
    update_run,
)
from log_parser import parse_grpd_log


mcp = FastMCP("GRPDExperimentServer")


@mcp.tool()
def create_experiment_run(
    experiment_name: str,
    case_name: str,
    case_dir: str = "",
    parameters: dict[str, Any] | None = None,
    git_commit: str = "",
    solver_type: str = "",
    kernel_type: str = "",
    material_type: str = "",
    notes: str = "",
    db_path: str = "",
) -> dict:
    """创建一次 GRPD 实验运行记录。"""
    return create_run(
        experiment_name=experiment_name,
        case_name=case_name,
        case_dir=case_dir,
        parameters=parameters or {},
        git_commit=git_commit,
        solver_type=solver_type,
        kernel_type=kernel_type,
        material_type=material_type,
        notes=notes,
        db_path=db_path,
    )


@mcp.tool()
def finish_experiment_run(
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
) -> dict:
    """结束一次运行并写入收敛、耗时和误差摘要。"""
    return finish_run(
        run_id=run_id,
        status=status,
        converged=converged,
        elapsed_seconds=elapsed_seconds,
        num_steps=num_steps,
        final_residual=final_residual,
        max_error_uy_percent=max_error_uy_percent,
        max_error_seqv_percent=max_error_seqv_percent,
        notes=notes,
        db_path=db_path,
    )


@mcp.tool()
def update_experiment_run(run_id: str, fields: dict[str, Any], db_path: str = "") -> dict:
    """局部更新运行记录字段。"""
    return update_run(run_id, db_path=db_path, **fields)


@mcp.tool()
def add_experiment_metric(
    run_id: str,
    name: str,
    value: float,
    unit: str = "",
    step: int | None = None,
    source: str = "",
    db_path: str = "",
) -> dict:
    """为运行记录追加一个指标，例如 residual、energy、max_damage。"""
    return record_metric(run_id, name, value, unit=unit, step=step, source=source, db_path=db_path)


@mcp.tool()
def add_experiment_artifact(
    run_id: str,
    kind: str,
    path: str,
    description: str = "",
    db_path: str = "",
) -> dict:
    """为运行记录追加结果文件路径，例如 vtk、log、plot、excel、zip。"""
    return record_artifact(run_id, kind, path, description=description, db_path=db_path)


@mcp.tool()
def import_grpd_log(run_id: str, log_file: str, db_path: str = "") -> dict:
    """解析 GRPD.log 并把收敛状态、步数、最终 residual 写入运行记录。"""
    parsed = parse_grpd_log(log_file)
    update = finish_run(
        run_id=run_id,
        status=parsed["status"] if parsed["status"] != "unknown" else "completed",
        converged=parsed["converged"],
        num_steps=parsed["num_steps"],
        final_residual=parsed["final_residual"],
        db_path=db_path,
    )
    record_artifact(run_id, "log", log_file, "Imported GRPD log", db_path=db_path)
    for item in parsed["residual_series"]:
        record_metric(
            run_id,
            "residual",
            item["value"],
            step=item["step"],
            source=log_file,
            db_path=db_path,
        )
    return {"success": True, "parsed": parsed, "run": update}


@mcp.tool()
def get_experiment_run(run_id: str, db_path: str = "") -> dict:
    """读取一次运行的完整记录，包括 metrics 和 artifacts。"""
    return get_run(run_id, db_path=db_path)


@mcp.tool()
def query_experiment_runs(
    experiment_name: str = "",
    case_name: str = "",
    status: str = "",
    limit: int = 20,
    db_path: str = "",
) -> list[dict]:
    """按实验名、算例名、状态查询历史运行。"""
    return query_runs(
        experiment_name=experiment_name,
        case_name=case_name,
        status=status,
        limit=limit,
        db_path=db_path,
    )


@mcp.tool()
def compare_experiment_parameter_sweep(
    experiment_name: str,
    parameter_name: str,
    metric_name: str = "final_residual",
    case_name: str = "",
    db_path: str = "",
) -> dict:
    """比较参数扫描结果，返回参数-指标点列和当前最优运行。"""
    return compare_parameter_sweep(
        experiment_name=experiment_name,
        parameter_name=parameter_name,
        metric_name=metric_name,
        case_name=case_name,
        db_path=db_path,
    )


@mcp.tool()
def summarize_experiment_convergence(
    experiment_name: str = "",
    case_name: str = "",
    db_path: str = "",
) -> dict:
    """汇总收敛率、失败数、最终 residual 范围和平均耗时。"""
    return summarize_convergence(experiment_name=experiment_name, case_name=case_name, db_path=db_path)


if __name__ == "__main__":
    mcp.run()
