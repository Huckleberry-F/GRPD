"""
server.py - GRPD MCP Server

提供 GRPD 项目自身的构建、运行和结果定位工具。
"""

from mcp.server.fastmcp import FastMCP

from grpd_runner import build_grpd as _build_grpd
from grpd_runner import find_target_vtk as _find_target_vtk
from grpd_runner import run_grpd_case as _run_grpd_case


mcp = FastMCP("GRPDServer")


@mcp.tool()
def build_grpd(build_dir: str, config: str = "Release") -> dict:
    """构建 GRPD，并返回构建状态与输出摘要。"""
    return _build_grpd(build_dir, config)


@mcp.tool()
def run_grpd_case(case_dir: str, executable: str = "") -> dict:
    """在指定算例目录运行 GRPD 可执行文件。"""
    return _run_grpd_case(case_dir, executable=executable)


@mcp.tool()
def find_latest_grpd_vtk(case_dir: str, substep: int = 0) -> dict:
    """在最新 Result_* 目录中查找目标 VTK；substep 为 0 时返回最新 VTK。"""
    return _find_target_vtk(case_dir, substep)


@mcp.tool()
def get_grpd_vtk_result(
    case_dir: str,
    vtk_file: str = "",
    build_dir: str = "",
    executable: str = "",
    substep: int = 10,
    build_config: str = "Release",
) -> dict:
    """
    获取 GRPD VTK 结果。若 vtk_file 已指定则跳过构建和运行；否则构建、运行并定位 VTK。
    """
    if vtk_file:
        return {"success": True, "skipped_run": True, "vtk_file": vtk_file, "substep": substep}

    if build_dir:
        build_result = _build_grpd(build_dir, build_config)
        if not build_result["success"]:
            return {"success": False, "stage": "build", "build": build_result}
    else:
        build_result = {"success": True, "skipped": True}

    run_result = _run_grpd_case(case_dir, executable=executable)
    if not run_result["success"]:
        return {"success": False, "stage": "grpd-run", "build": build_result, "grpd": run_result}

    vtk_result = _find_target_vtk(case_dir, substep)
    return {"success": True, "skipped_run": False, "build": build_result, "grpd": run_result, **vtk_result}


if __name__ == "__main__":
    mcp.run()
