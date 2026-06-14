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
    """运行 GRPD 算例。

    stdout/stderr 重定向到临时文件而非管道，以避免 GRPD.exe 内部通过
    std::system 调用 Python 时因子进程继承管道句柄而产生的 I/O 死锁。
    """
    if not os.path.isdir(case_dir):
        raise FileNotFoundError(f"GRPD case directory not found: {case_dir}")
    yaml_path = os.path.join(case_dir, "PD.yaml")
    if not os.path.isfile(yaml_path):
        raise FileNotFoundError(f"PD.yaml not found in GRPD case directory: {case_dir}")

    exe_path = resolve_grpd_executable(grpd_root=grpd_root, executable=executable)
    start_time = time.time()

    import sys
    env = os.environ.copy()
    env["GRPD_PYTHON_EXE"] = sys.executable
    python_dir = os.path.dirname(sys.executable)
    scripts_dir = os.path.join(python_dir, "Scripts")
    path_entries = [python_dir, scripts_dir]
    original_path = env.get("PATH", "")
    if original_path:
        path_entries.append(original_path)
    env["PATH"] = os.pathsep.join(path_entries)

    # 使用临时文件接收输出，切断管道继承链
    stdout_path = os.path.join(case_dir, "_grpd_stdout.log")
    stderr_path = os.path.join(case_dir, "_grpd_stderr.log")
    try:
        with open(stdout_path, "w", encoding="utf-8") as f_out, \
             open(stderr_path, "w", encoding="utf-8") as f_err:
            completed = subprocess.run(
                [exe_path],
                cwd=case_dir,
                check=False,
                stdin=subprocess.DEVNULL,
                stdout=f_out,
                stderr=f_err,
                env=env,
            )

        # 运行结束后读回输出内容
        with open(stdout_path, "r", encoding="utf-8", errors="replace") as f:
            stdout_text = f.read()
        with open(stderr_path, "r", encoding="utf-8", errors="replace") as f:
            stderr_text = f.read()
    finally:
        # 清理临时日志文件
        for p in (stdout_path, stderr_path):
            try:
                os.unlink(p)
            except OSError:
                pass

    return {
        "success": completed.returncode == 0,
        "returncode": completed.returncode,
        "elapsed_seconds": time.time() - start_time,
        "executable": exe_path,
        "stdout_tail": stdout_text[-4000:],
        "stderr_tail": stderr_text[-4000:],
    }


def find_target_vtk(case_dir: str, substep: int = 0) -> dict:
    result_folders = glob.glob(os.path.join(case_dir, "Result_*"))
    if not result_folders:
        raise RuntimeError(f"No Result_* folders found in {case_dir}.")

    latest_folder = max(result_folders, key=os.path.getctime)
    warning = ""
    if substep:
        import re
        vtks = []
        for file in glob.glob(os.path.join(latest_folder, "*.vtk")):
            m = re.search(r"step(\d+)_", os.path.basename(file))
            if m and int(m.group(1)) == substep:
                vtks.append(file)
        if not vtks:
            warning = (
                f"No VTK file matched step{substep}; "
                "falling back to the newest VTK in the latest result folder."
            )
            vtks = glob.glob(os.path.join(latest_folder, "*.vtk"))
    else:
        vtks = glob.glob(os.path.join(latest_folder, "*.vtk"))

    if not vtks:
        raise RuntimeError(f"No VTK files found in the latest result folder: {latest_folder}.")

    target_vtk = max(vtks, key=os.path.getctime)
    
    import re
    physical_time = 0.0
    match = re.search(r"_t([\d.]+)\.vtk$", os.path.basename(target_vtk))
    if match:
        try:
            physical_time = float(match.group(1))
        except ValueError:
            pass

    return {
        "success": True,
        "vtk_file": target_vtk,
        "result_folder": latest_folder,
        "substep": substep,
        "physical_time": physical_time,
        "warning": warning,
    }
