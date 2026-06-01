# -*- coding: utf-8 -*-
"""
server.py - 用于近场动力学材料本构单点联合校验的 MATLAB MCP 服务端。"""

from __future__ import annotations

import os
import sys
import json
import subprocess
from typing import Any

import yaml
from mcp.server.fastmcp import FastMCP

from matlab_runner import matlab_available, run_custom_matlab_script  # type: ignore
from m_generator import generate_matlab_constitutive_script
from result_parser import compare_results_and_plot

# 动态获取项目根目录
PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))

CONFIG_FILE = os.path.join(os.path.dirname(__file__), "config.yaml")
with open(CONFIG_FILE, "r", encoding="utf-8") as file:
    config = yaml.safe_load(file) or {}

MATLAB_EXECUTABLE = config.get("matlab_executable", "matlab")
DEFAULTS = config.get("defaults", {})
import sqlite3

DB_FILE = os.path.join(os.path.dirname(__file__), "validation_history.db")

def get_next_work_dir(base_dir: str, prefix: str = "run_") -> str:
    """在 base_dir 下寻找下一个递增编号的文件夹路径"""
    os.makedirs(base_dir, exist_ok=True)
    existing_folders = [f for f in os.listdir(base_dir) if os.path.isdir(os.path.join(base_dir, f))]

    max_idx = 0
    for folder in existing_folders:
        if folder.startswith(prefix):
            try:
                idx = int(folder[len(prefix):])
                if idx > max_idx:
                    max_idx = idx
            except ValueError:
                pass

    next_idx = max_idx + 1
    return os.path.join(base_dir, f"{prefix}{next_idx:04d}")


def save_matlab_validation_to_db(db_path: str, parameters: dict, comparison_res: dict):
    """将对标校验结果记录写入 SQLite 数据库"""
    conn = sqlite3.connect(db_path)
    cursor = conn.cursor()
    cursor.execute("""
        CREATE TABLE IF NOT EXISTS matlab_constitutive_validation (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
            model TEXT,
            is_large INTEGER,
            youngs_modulus REAL,
            poissons_ratio REAL,
            yield_stress REAL,
            max_err_stress REAL,
            max_err_eqps REAL,
            max_err_damage REAL,
            passed INTEGER,
            work_dir TEXT,
            plot_file TEXT,
            xlsx_file TEXT,
            zip_file TEXT
        )
    """)

    model = parameters.get("model", "J2VoceLemaitre")
    is_large = 1 if parameters.get("LargeDeformation", False) else 0
    E = parameters.get("YoungsModulus", 0.0)
    nu = parameters.get("PoissonsRatio", 0.0)
    sigma0 = parameters.get("YieldStress", 0.0)

    cursor.execute("""
        INSERT INTO matlab_constitutive_validation
        (model, is_large, youngs_modulus, poissons_ratio, yield_stress,
         max_err_stress, max_err_eqps, max_err_damage, passed, work_dir, plot_file, xlsx_file, zip_file)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    """, (
        model, is_large, E, nu, sigma0,
        comparison_res.get("max_err_stress", -1.0),
        comparison_res.get("max_err_eqps", -1.0),
        comparison_res.get("max_err_damage", -1.0),
        1 if comparison_res.get("passed", False) else 0,
        comparison_res.get("work_dir", ""),
        comparison_res.get("plot_file", ""),
        comparison_res.get("xlsx_file", ""),
        comparison_res.get("zip_file", "")
    ))
    conn.commit()
    conn.close()


mcp = FastMCP("PointIntegrationMatlabServer")



def compile_and_locate_cpp_test(root_dir: str) -> str:
    """自动编译 C++ test_constitutive 并定位其可执行文件路径"""
    build_dir = os.path.join(root_dir, "build")
    if not os.path.exists(build_dir):
        raise FileNotFoundError(f"未找到 CMake 构建目录: {build_dir}。请先运行 cmake 配置！")

    # 编译 test_constitutive
    cmd = ["cmake", "--build", build_dir, "--config", "Release", "--target", "test_constitutive"]
    res = subprocess.run(cmd, capture_output=True, text=True)
    if res.returncode != 0:
        raise RuntimeError(f"C++ 校验程序编译失败!\nStdout: {res.stdout}\nStderr: {res.stderr}")

    possible_paths = [
        os.path.join(root_dir, "bin", "Release", "test_constitutive.exe"),
        os.path.join(root_dir, "bin", "release", "test_constitutive.exe"),
        os.path.join(root_dir, "bin", "test_constitutive.exe"),
        os.path.join(root_dir, "build", "bin", "Release", "test_constitutive.exe"),
        os.path.join(root_dir, "build", "bin", "test_constitutive.exe"),
    ]
    for path in possible_paths:
        if os.path.exists(path):
            return path
    raise FileNotFoundError("在编译成功后未找到 test_constitutive.exe 可执行文件！")


@mcp.tool()
def check_matlab_available() -> dict[str, Any]:
    """检查 MATLAB 可执行文件是否可用"""
    return matlab_available(MATLAB_EXECUTABLE)


@mcp.tool()
def run_constitutive_validation(
    parameters: dict[str, Any],
    F_path: list[list[float]],
    work_dir: str = "",
) -> dict[str, Any]:
    """
    一键执行 GRPD C++ 材料本构与 MATLAB 参考解析解的单点联合对标校验。
    自动完成 C++ 编译、运行、生成 MATLAB 积分脚本与求解，以及双端结果误差比对和绘图。
    参数:
    - parameters: 材料参数字典 (必须包含 model 等，例如 "model": "J2VoceLemaitre")
    - F_path: 9分量变形梯度路径序列，步数 x 9。例如: [[F11, F12, F13, F21, F22, F23, F31, F32, F33], ...]
    - work_dir: 可选，校验工作目录。若不指定，默认为 <project_root>/.agents/mcp/matlab-mcp-server/work_dir
    """
    # 1. 确定工作目录
    base_work_dir = os.path.join(os.path.dirname(__file__), "work_dir")
    if not work_dir:
        work_dir = get_next_work_dir(base_work_dir)
    else:
        work_dir = os.path.abspath(work_dir)
    os.makedirs(work_dir, exist_ok=True)

    try:
        # 2. 编译并定位 C++ 可执行文件
        exe_path = compile_and_locate_cpp_test(PROJECT_ROOT)

        # 3. 将输入数据写入 input_path.json
        input_data = {
            "parameters": parameters,
            "F_path": F_path
        }
        input_json_path = os.path.join(work_dir, "input_path.json")
        with open(input_json_path, "w", encoding="utf-8") as f:
            json.dump(input_data, f, indent=2)

        # 4. 运行 C++ 求解器生成 cpp_results.json
        cmd = [exe_path, input_json_path]
        cpp_run_res = subprocess.run(cmd, cwd=work_dir, capture_output=True, text=True)
        if cpp_run_res.returncode != 0:
            return {
                "success": False,
                "message": f"C++ 校验程序运行崩溃!\nStdout: {cpp_run_res.stdout}\nStderr: {cpp_run_res.stderr}"
            }

        # 5. 动态生成 MATLAB 积分脚本
        matlab_script = generate_matlab_constitutive_script(
            parameters=parameters,
            input_json_name="input.json",
            output_json_name="output.json"
        )

        # 6. 调用 MATLAB 求解器运行积分计算，生成 output.json
        timeout_seconds = int(DEFAULTS.get("timeout_seconds", 300))
        matlab_res = run_custom_matlab_script(
            matlab_executable=MATLAB_EXECUTABLE,
            script_content=matlab_script,
            input_data=input_data,
            work_dir=work_dir,
            output_filename="output.json",
            timeout_seconds=timeout_seconds,
            keep_temp_files=True
        )

        if isinstance(matlab_res, dict) and (not matlab_res.get("success", True) or "error" in matlab_res):
            return {
                "success": False,
                "message": f"MATLAB 积分计算运行失败: {matlab_res.get('message', '未知错误')}",
                "detail": matlab_res
            }

        # 7. 调用 result_parser 比对两端输出
        cpp_json_path = os.path.join(work_dir, "cpp_results.json")
        matlab_json_path = os.path.join(work_dir, "output.json")
        output_plot_path = os.path.join(work_dir, "Comparison_Plot.png")

        # 容差约束：应力最大绝对误差容差设置为 1.0e-5 Pa，损伤度与等效塑性应变设置为 1.0e-10
        tol_stress = 1.0e-5
        tol_state = 1.0e-10

        comparison_res = compare_results_and_plot(
            cpp_json_path=cpp_json_path,
            matlab_json_path=matlab_json_path,
            output_plot_path=output_plot_path,
            tol_stress=tol_stress,
            tol_state=tol_state
        )

        # 将工作目录和结果等一并回传
        comparison_res["work_dir"] = work_dir
        comparison_res["cpp_results_file"] = cpp_json_path
        comparison_res["matlab_results_file"] = matlab_json_path
        comparison_res["plot_file"] = output_plot_path

        # 合并 MATLAB 生成的 XLSX、参考 plot 与 ZIP 路径
        if isinstance(matlab_res, dict):
            if "xlsx_path" in matlab_res:
                comparison_res["xlsx_file"] = matlab_res["xlsx_path"]
            if "png_path" in matlab_res:
                comparison_res["matlab_plot_file"] = matlab_res["png_path"]
            if "zip_path" in matlab_res:
                comparison_res["zip_file"] = matlab_res["zip_path"]

        # 将校验结果存入 SQLite 数据库
        try:
            save_matlab_validation_to_db(DB_FILE, parameters, comparison_res)
        except Exception as db_err:
            comparison_res["db_warning"] = f"写入 SQLite 数据库失败: {str(db_err)}"

        return comparison_res

    except Exception as e:
        return {
            "success": False,
            "message": f"校验管道发生未捕获异常: {str(e)}"
        }


@mcp.tool()
def run_custom_script(
    script_content: str,
    input_data: dict[str, Any],
    work_dir: str = "",
    output_filename: str = "output.json",
) -> dict[str, Any]:
    """
    运行自定义的任意 MATLAB 脚本，并自动读回指定的输出 JSON 结果。
    用于完全通用的本构及其他物理过程的 MATLAB 联合校验。
    """
    if not work_dir:
        base_work_dir = os.path.join(os.path.dirname(__file__), "work_dir")
        work_dir = get_next_work_dir(base_work_dir)
    else:
        work_dir = os.path.abspath(work_dir)
    os.makedirs(work_dir, exist_ok=True)

    timeout_seconds = int(DEFAULTS.get("timeout_seconds", 300))
    keep_temp_files = bool(DEFAULTS.get("keep_temp_files", False))
    return run_custom_matlab_script(
        matlab_executable=MATLAB_EXECUTABLE,
        script_content=script_content,
        input_data=input_data,
        work_dir=work_dir,
        output_filename=output_filename,
        timeout_seconds=timeout_seconds,
        keep_temp_files=keep_temp_files,
    )


if __name__ == "__main__":
    mcp.run()
