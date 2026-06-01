"""FastMCP facade for ANSYS-side validation tools."""

from __future__ import annotations

import os
import sys

from mcp.server.fastmcp import FastMCP

SERVER_DIR = os.path.dirname(os.path.abspath(__file__))


def _activate_local_src_package() -> None:
    """Prefer this service's local ``src`` package during facade imports."""
    local_src = os.path.join(SERVER_DIR, "src")
    existing = sys.modules.get("src")
    existing_file = getattr(existing, "__file__", "") if existing else ""
    if existing and not os.path.abspath(existing_file).startswith(local_src):
        for name in list(sys.modules):
            if name == "src" or name.startswith("src."):
                del sys.modules[name]

    if SERVER_DIR in sys.path:
        sys.path.remove(SERVER_DIR)
    sys.path.insert(0, SERVER_DIR)


_activate_local_src_package()

from src.service import (
    generate_ansys_apdl_from_yaml as _generate_ansys_apdl_from_yaml,
    get_ansys_solve_status as _get_ansys_solve_status,
    get_ansys_text_results as _get_ansys_text_results,
    list_ansys_files as _list_ansys_files,
    run_ansys_mac as _run_ansys_mac,
    run_ansys_yaml_case as _run_ansys_yaml_case,
)


mcp = FastMCP("AnsysServer")


@mcp.tool()
def run_ansys_mac(
    mac_file: str,
    work_dir: str = "",
    job_name: str = "",
    parameters: dict = None,
) -> dict:
    """
    运行 ANSYS MAPDL 批处理任务。支持动态向 APDL 注入参数。
    :param mac_file: .mac 脚本文件的绝对路径。
    :param work_dir: 工作目录绝对路径（留空则默认为 mac_file 同目录）。
    :param job_name: 任务名称（留空则默认使用 mac_file 的文件名）。
    :param parameters: 字典形式的参数集，会在执行前以 Wrapper Macro 形式注入 APDL。
    """
    return _run_ansys_mac(mac_file, work_dir, job_name, parameters)


@mcp.tool()
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
    """
    根据 GRPD 的 PD.yaml 生成 ANSYS APDL 验证宏，并包含 SAVE 命令生成 .db 文件。
    坐标参数为 APDL 未收到动态参数时使用的默认采样路径。
    """
    return _generate_ansys_apdl_from_yaml(
        yaml_file,
        output_mac,
        substep,
        job_name,
        start_x,
        end_x,
        start_y,
        end_y,
    )


@mcp.tool()
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
    """由 GRPD PD.yaml 生成 APDL，运行 ANSYS，并返回 txt/db/out/err 等结果路径。"""
    return _run_ansys_yaml_case(
        yaml_file,
        work_dir,
        substep,
        start_x,
        end_x,
        start_y,
        end_y,
        job_name,
    )


@mcp.tool()
def get_ansys_solve_status(out_file: str) -> dict:
    """
    解析 ANSYS .out 日志文件，获取求解收敛状态、警告和错误信息。
    :param out_file: .out 文件的绝对路径。
    """
    return _get_ansys_solve_status(out_file)


@mcp.tool()
def get_ansys_text_results(txt_file: str) -> dict:
    """
    解析 ANSYS PRVAR/PRNSOL 导出的纯文本数据文件 (.txt)。
    返回列名、数据行数、极值以及预览数据，供进一步数值分析。
    :param txt_file: .txt 结果文件的绝对路径。
    """
    return _get_ansys_text_results(txt_file)


@mcp.tool()
def list_ansys_files(work_dir: str) -> dict:
    """
    列出指定工作目录中的所有 ANSYS 结果文件和日志文件。
    :param work_dir: 工作目录 of the absolute path.
    """
    return _list_ansys_files(work_dir)


if __name__ == "__main__":
    mcp.run()
