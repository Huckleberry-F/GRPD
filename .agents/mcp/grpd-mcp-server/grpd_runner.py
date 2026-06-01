import glob
import os
import subprocess
import time
from typing import Optional


def find_grpd_root(start_path: Optional[str] = None) -> str:
    path = os.path.abspath(start_path or __file__)
    current = path if os.path.isdir(path) else os.path.dirname(path)
    while True:
        if (
            os.path.exists(os.path.join(current, "AGENTS.md"))
            and os.path.exists(os.path.join(current, "CMakeLists.txt"))
        ):
            return current
        parent = os.path.dirname(current)
        if parent == current:
            raise RuntimeError(f"Cannot locate GRPD repository root from {path}.")
        current = parent


def resolve_grpd_executable(grpd_root: Optional[str] = None, executable: str = "") -> str:
    if executable:
        if not os.path.exists(executable):
            raise FileNotFoundError(f"GRPD executable not found: {executable}")
        return os.path.abspath(executable)

    root = grpd_root or find_grpd_root()
    candidates = [
        os.path.join(root, "bin", "release", "GRPD.exe"),
        os.path.join(root, "bin", "Release", "GRPD.exe"),
        os.path.join(root, "bin", "GRPD.exe"),
        os.path.join(root, "bin", "PD.exe"),
    ]
    for candidate in candidates:
        if os.path.exists(candidate):
            return candidate
    raise FileNotFoundError("Cannot locate GRPD executable in bin/release, bin/Release, or bin.")


def build_grpd(build_dir: str, config: str = "Release") -> dict:
    if not os.path.isdir(build_dir):
        raise FileNotFoundError(f"CMake build directory not found: {build_dir}")

    start_time = time.time()
    completed = subprocess.run(
        ["cmake", "--build", ".", "--config", config],
        cwd=build_dir,
        check=False,
        text=True,
        capture_output=True,
    )
    return {
        "success": completed.returncode == 0,
        "returncode": completed.returncode,
        "elapsed_seconds": time.time() - start_time,
        "stdout_tail": completed.stdout[-4000:],
        "stderr_tail": completed.stderr[-4000:],
    }


def run_grpd_case(case_dir: str, executable: str = "", grpd_root: Optional[str] = None) -> dict:
    if not os.path.isdir(case_dir):
        raise FileNotFoundError(f"GRPD case directory not found: {case_dir}")

    exe_path = resolve_grpd_executable(grpd_root=grpd_root, executable=executable)
    start_time = time.time()
    completed = subprocess.run(
        [exe_path],
        cwd=case_dir,
        check=False,
        text=True,
        capture_output=True,
    )
    return {
        "success": completed.returncode == 0,
        "returncode": completed.returncode,
        "elapsed_seconds": time.time() - start_time,
        "executable": exe_path,
        "stdout_tail": completed.stdout[-4000:],
        "stderr_tail": completed.stderr[-4000:],
    }


def find_target_vtk(case_dir: str, substep: int = 0) -> dict:
    result_folders = glob.glob(os.path.join(case_dir, "Result_*"))
    if not result_folders:
        raise RuntimeError(f"No Result_* folders found in {case_dir}.")

    latest_folder = max(result_folders, key=os.path.getctime)
    warning = ""
    if substep:
        pattern = f"*step{substep:05d}*.vtk"
        vtks = glob.glob(os.path.join(latest_folder, pattern))
        if not vtks:
            warning = (
                f"No VTK file matched step{substep:05d}; "
                "falling back to the newest VTK in the latest result folder."
            )
            vtks = glob.glob(os.path.join(latest_folder, "*.vtk"))
    else:
        vtks = glob.glob(os.path.join(latest_folder, "*.vtk"))

    if not vtks:
        raise RuntimeError(f"No VTK files found in the latest result folder: {latest_folder}.")

    target_vtk = max(vtks, key=os.path.getctime)
    return {
        "success": True,
        "vtk_file": target_vtk,
        "result_folder": latest_folder,
        "substep": substep,
        "warning": warning,
    }
