"""
matlab_runner.py - MATLAB batch launcher for generated point-integration scripts.
"""

from __future__ import annotations

import json
import os
import shutil
import subprocess
import tempfile
from pathlib import Path
from typing import Any


def matlab_available(matlab_executable: str) -> dict[str, Any]:
    resolved = shutil.which(matlab_executable) if not os.path.isabs(matlab_executable) else matlab_executable
    exists = bool(resolved and os.path.exists(resolved))
    return {"available": exists, "executable": matlab_executable, "resolved": resolved or ""}



def run_custom_matlab_script(
    matlab_executable: str,
    script_content: str,
    input_data: dict[str, Any],
    work_dir: str = "",
    output_filename: str = "output.json",
    timeout_seconds: int = 120,
    keep_temp_files: bool = False,
) -> dict[str, Any]:
    """
    运行自定义的 MATLAB 脚本，并自动读回指定输出 JSON 的内容�?    """
    availability = matlab_available(matlab_executable)
    if not availability["available"]:
        return {"success": False, "message": "MATLAB executable not found.", "availability": availability}

    is_temp = False
    if not work_dir:
        is_temp = True
        work_dir = tempfile.mkdtemp(prefix="grpd_matlab_custom_")
    else:
        work_dir = os.path.abspath(work_dir)
        os.makedirs(work_dir, exist_ok=True)

    input_path = os.path.join(work_dir, "input.json")
    output_path = os.path.join(work_dir, output_filename)
    script_path = os.path.join(work_dir, "run_point_integration.m")

    with open(input_path, "w", encoding="utf-8") as file:
        json.dump(input_data, file)

    with open(script_path, "w", encoding="utf-8") as file:
        file.write(script_content)

    try:
        process = subprocess.run(
            [availability["resolved"], "-batch", f"run('{script_path}')"],
            cwd=work_dir,
            capture_output=True,
            text=True,
            timeout=timeout_seconds,
        )
        if process.returncode != 0:
            return {
                "success": False,
                "message": f"MATLAB returned {process.returncode}.",
                "stdout_tail": process.stdout[-4000:] if process.stdout else "",
                "stderr_tail": process.stderr[-4000:] if process.stderr else "",
                "work_dir": work_dir if (keep_temp_files or not is_temp) else "",
            }

        if not os.path.exists(output_path):
            return {
                "success": False,
                "message": f"MATLAB script finished but output file '{output_filename}' was not found.",
                "stdout_tail": process.stdout[-2000:] if process.stdout else "",
                "stderr_tail": process.stderr[-2000:] if process.stderr else "",
            }

        with open(output_path, "r", encoding="utf-8") as file:
            res_data = json.load(file)

        if not is_temp:
            xlsx_path = os.path.join(work_dir, "constitutive_results.xlsx")
            png_path = os.path.join(work_dir, "constitutive_plot.png")
            if os.path.exists(xlsx_path):
                res_data["xlsx_path"] = xlsx_path
            if os.path.exists(png_path):
                res_data["png_path"] = png_path

            import zipfile
            zip_path = os.path.join(work_dir, "matlab_constitutive_results.zip")
            files_to_zip = []
            for f in os.listdir(work_dir):
                fp = os.path.join(work_dir, f)
                if os.path.isfile(fp) and f != "matlab_constitutive_results.zip":
                    files_to_zip.append(fp)
            try:
                with zipfile.ZipFile(zip_path, 'w', zipfile.ZIP_DEFLATED) as zipf:
                    for fp in files_to_zip:
                        zipf.write(fp, os.path.basename(fp))
                res_data["zip_path"] = zip_path
            except Exception:
                pass

        return res_data
    except subprocess.TimeoutExpired:
        return {"success": False, "message": f"MATLAB timed out after {timeout_seconds}s.", "work_dir": work_dir}
    finally:
        if is_temp and not keep_temp_files:
            shutil.rmtree(work_dir, ignore_errors=True)


def safe_temp_root() -> str:
    return str(Path(tempfile.gettempdir()).resolve())
