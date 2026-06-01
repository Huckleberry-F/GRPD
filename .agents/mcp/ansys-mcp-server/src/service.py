"""Service-level operations behind the ANSYS MCP facade."""

from __future__ import annotations

import os
from typing import Any

from .generator import generate_apdl_from_yaml as _generate_apdl_from_yaml
from .history import save_ansys_validation_to_db
from .paths import (
    allowed_directories_from_config,
    default_work_dir_base,
    get_next_work_dir,
    load_config,
    validation_db_file,
)
from .result_parser import ResultParser
from .runner import AnsysRunner


CONFIG = load_config()
DB_FILE = validation_db_file()
RUNNER = AnsysRunner(
    executable=CONFIG.get("ansys_executable", ""),
    num_processors=CONFIG.get("defaults", {}).get("num_processors", 4),
    memory_mb=CONFIG.get("defaults", {}).get("memory_mb", 2048),
    timeout_seconds=CONFIG.get("defaults", {}).get("timeout_seconds", 3600),
    allowed_directories=allowed_directories_from_config(CONFIG),
)


def run_ansys_mac(
    mac_file: str,
    work_dir: str = "",
    job_name: str = "",
    parameters: dict | None = None,
) -> dict:
    """Run an ANSYS MAPDL macro file and return JSON-safe metadata."""
    if not work_dir:
        work_dir = os.path.dirname(mac_file)
    result = RUNNER.run_mac_file(mac_file, work_dir, job_name, parameters=parameters)
    return {
        "success": result.success,
        "job_name": result.job_name,
        "work_dir": result.work_dir,
        "elapsed_seconds": result.elapsed_seconds,
        "message": result.message,
        "error_log_tail": result.error_log[-2000:] if result.error_log else "",
        "result_files_count": len(result.result_files),
        "result_files": result.result_files,
    }


def generate_ansys_apdl_from_yaml(
    yaml_file: str,
    output_mac: str,
    substep: int = 0,
    job_name: str = "ansys_smoke_test",
    start_x: float = 0.0,
    end_x: float = 0.0,
    start_y: float = 0.0,
    end_y: float = 0.0,
) -> dict:
    """Generate an ANSYS APDL validation macro from a GRPD ``PD.yaml``."""
    result = _generate_apdl_from_yaml(
        yaml_file,
        output_mac,
        job_name=job_name,
        result_substep=substep,
        default_start_x=start_x,
        default_end_x=end_x if end_x != 0.0 else start_x,
        default_start_y=start_y,
        default_end_y=end_y if end_y != 0.0 else None,
    )
    result["ansys_txt_file"] = os.path.join(
        os.path.dirname(os.path.abspath(output_mac)),
        f"ansys_val_results_sub{substep}.txt" if substep else "ansys_val_results.txt",
    )
    result["substep"] = substep
    return result


def run_ansys_yaml_case(
    yaml_file: str,
    work_dir: str = "",
    substep: int = 0,
    start_x: float = 1.0,
    end_x: float = 1.0,
    start_y: float = 0.0,
    end_y: float = 0.0,
    job_name: str = "ansys_smoke_test",
) -> dict:
    """Generate APDL from GRPD YAML, run ANSYS, and return result paths."""
    if not work_dir:
        work_dir = get_next_work_dir(default_work_dir_base())
    else:
        work_dir = os.path.abspath(work_dir)
    os.makedirs(work_dir, exist_ok=True)

    mac_file = os.path.join(work_dir, f"{job_name}.mac")
    _generate_apdl_from_yaml(
        yaml_file,
        mac_file,
        job_name=job_name,
        result_substep=substep,
        default_start_x=start_x,
        default_end_x=end_x,
        default_start_y=start_y,
        default_end_y=end_y if end_y != 0.0 else None,
    )

    parameters: dict[str, Any] = {"START_X": start_x, "END_X": end_x}
    if start_y != 0.0:
        parameters["START_Y"] = start_y
    if end_y != 0.0:
        parameters["END_Y"] = end_y
    if substep:
        parameters["SUBSTEP"] = substep

    result = RUNNER.run_mac_file(mac_file, work_dir, job_name, parameters=parameters)
    ansys_txt_file = os.path.join(
        work_dir,
        f"ansys_val_results_sub{substep}.txt" if substep else "ansys_val_results.txt",
    )
    db_file = os.path.join(work_dir, f"{job_name}.db")
    out_file = os.path.join(work_dir, f"{job_name}.out")
    err_file = os.path.join(work_dir, f"{job_name}.err")

    text_result = ResultParser.parse_prvar_output(ansys_txt_file)
    txt_ok = bool(text_result.get("success"))
    db_exists = os.path.exists(db_file)
    has_fatal_error = "*** ERROR ***" in (result.error_log or "")
    success = not has_fatal_error and txt_ok and db_exists

    warnings: list[str] = []
    if not result.success:
        warnings.append(result.message)
    if not db_exists:
        warnings.append(f"ANSYS database file was not found: {db_file}")
    if not txt_ok:
        warnings.append(
            text_result.get(
                "message",
                f"ANSYS text result was not parseable: {ansys_txt_file}",
            )
        )

    res_dict = {
        "success": success,
        "work_dir": work_dir,
        "mac_file": mac_file,
        "ansys_txt_file": ansys_txt_file,
        "db_file": db_file,
        "db_exists": db_exists,
        "out_file": out_file,
        "err_file": err_file,
        "result_files": result.result_files,
        "elapsed_seconds": result.elapsed_seconds,
        "message": result.message,
        "warnings": warnings,
        "error_log_tail": result.error_log[-2000:] if result.error_log else "",
        "text_result": text_result,
    }

    try:
        save_ansys_validation_to_db(DB_FILE, res_dict, yaml_file, substep, job_name)
    except Exception as db_err:
        res_dict["db_warning"] = f"Failed to write SQLite database: {db_err}"

    return res_dict


def get_ansys_solve_status(out_file: str) -> dict:
    """Parse ANSYS ``.out`` solve status."""
    return ResultParser.parse_out_file(out_file)


def get_ansys_text_results(txt_file: str) -> dict:
    """Parse ANSYS PRVAR/PRNSOL text output."""
    return ResultParser.parse_prvar_output(txt_file)


def list_ansys_files(work_dir: str) -> dict:
    """List ANSYS result files in a work directory."""
    return ResultParser.list_result_files(work_dir)
