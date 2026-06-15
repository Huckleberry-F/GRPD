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


def _get_effective_suffix(yaml_file: str, substep: int, time: float) -> str:
    effective_time = time
    if effective_time <= 0.0 and substep > 0:
        try:
            import yaml
            with open(yaml_file, "r", encoding="utf-8") as f:
                y_data = yaml.safe_load(f) or {}
            dt = float(y_data.get("Solver", {}).get("TimeStep_dt", 0.0))
            if dt > 0.0:
                effective_time = substep * dt
        except Exception:
            pass
    return f"_t{effective_time}" if effective_time > 0.0 else (f"_sub{substep}" if substep else "")


def run_ansys_mac(
    mac_file: str,
    work_dir: str = "",
    job_name: str = "",
    parameters: dict | None = None,
) -> dict:
    """Run an ANSYS MAPDL macro file and return JSON-safe metadata."""
    config = load_config()
    RUNNER.executable = config.get("ansys_executable", RUNNER.executable)
    RUNNER.allowed_directories = allowed_directories_from_config(config)
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
    time: float = 0.0,
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
        result_time=time,
        default_start_x=start_x,
        default_end_x=end_x if end_x != 0.0 else start_x,
        default_start_y=start_y,
        default_end_y=end_y if end_y != 0.0 else None,
    )
    suffix = _get_effective_suffix(yaml_file, substep, time)
    result["ansys_txt_file"] = os.path.join(
        os.path.dirname(os.path.abspath(output_mac)),
        f"ansys_val_results{suffix}.txt",
    )
    result["substep"] = substep
    result["time"] = time
    return result


def run_ansys_yaml_case(
    yaml_file: str,
    work_dir: str = "",
    substep: int = 0,
    time: float = 0.0,
    start_x: float = 1.0,
    end_x: float = 1.0,
    start_y: float = 0.0,
    end_y: float = 0.0,
    job_name: str = "ansys_smoke_test",
    template_name: str = "",
) -> dict:
    """Generate APDL from GRPD YAML or templates, run ANSYS, and return result paths."""
    config = load_config()
    RUNNER.executable = config.get("ansys_executable", RUNNER.executable)
    RUNNER.allowed_directories = allowed_directories_from_config(config)
    if not work_dir:
        work_dir = get_next_work_dir(default_work_dir_base())
    else:
        work_dir = os.path.abspath(work_dir)
    os.makedirs(work_dir, exist_ok=True)

    mac_file = os.path.join(work_dir, f"{job_name}.mac")
    
    # 内置模板加载路径
    templates_dir = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "templates")
    template_path = os.path.join(templates_dir, f"{template_name}.mac") if template_name else ""

    if template_name and os.path.isfile(template_path):
        import sys
        print(f"[Ansys Service] Using built-in APDL template: {template_path}", file=sys.stderr)
        # 解析 YAML 并填入模板参数
        import yaml
        with open(yaml_file, "r", encoding="utf-8") as f:
            yaml_data = yaml.safe_load(f) or {}
        
        part = yaml_data.get("Parts", [{}])[0]
        material = yaml_data.get("Materials", [{}])[0]
        solver = yaml_data.get("Solver", {})
        bcs = yaml_data.get("BoundaryConditions", [])
        
        # 热源参数提取
        flux_bc = next((bc for bc in bcs if bc.get("Type", "").upper() == "FLUX"), {})
        temp_bc = next((bc for bc in bcs if bc.get("Type", "").upper() in {"T", "TEMP"}), {})
        
        flux_box = flux_bc.get("Box", [0,0,0,0,0,0])
        temp_box = temp_bc.get("Box", [0,0,0,0,0,0])
        
        load_steps = solver.get("LoadSteps", [{}])
        raw_substeps = load_steps[0].get("NumSubsteps", 20) if load_steps else 20
        # 优化隐式求解步数，避免跟随 GRPD 显式细密步长
        num_substeps = min(100, raw_substeps) if raw_substeps > 100 else raw_substeps
        
        with open(template_path, "r", encoding="utf-8") as ft:
            macro_content = ft.read()
            
        # 参数替换
        replacements = {
            "MAT_K": str(material.get("Conductivity", 45.0)),
            "MAT_RHO": str(material.get("Density", 7.85e-9)),
            "MAT_C": str(material.get("HeatCapacity", 4.6e8)),
            "PART_X1": "0.0",
            "PART_X2": str(part.get("Length", 5.0)),
            "PART_Y1": "0.0",
            "PART_Y2": str(part.get("Width", 10.0)),
            "PART_DX": str(part.get("dx", 0.1)),
            "BC_FLUX_XMIN": str(flux_box[0]),
            "BC_FLUX_XMAX": str(flux_box[1]),
            "BC_FLUX_VAL": str(flux_bc.get("Value", 100.0)),
            "BC_TEMP_XMIN": str(temp_box[0]),
            "BC_TEMP_XMAX": str(temp_box[1]),
            "BC_TEMP_VAL": str(temp_bc.get("Value", 100.0)),
            "SOL_SUBSTEPS": str(num_substeps),
            "SAMPLE_START_X": str(start_x),
            "SAMPLE_START_Y": str(start_y),
            "SAMPLE_END_X": str(end_x),
            "SAMPLE_END_Y": str(end_y),
        }
        
        for k, v in replacements.items():
            macro_content = macro_content.replace(k, v)
            
        with open(mac_file, "w", encoding="utf-8") as fm:
            fm.write(macro_content)
    else:
        _generate_apdl_from_yaml(
            yaml_file,
            mac_file,
            job_name=job_name,
            result_substep=substep,
            result_time=time,
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
    if time > 0.0:
        parameters["TIME"] = time

    result = RUNNER.run_mac_file(mac_file, work_dir, job_name, parameters=parameters)
    suffix = _get_effective_suffix(yaml_file, substep, time)
    ansys_txt_file = os.path.join(work_dir, f"ansys_val_results{suffix}.txt")
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
