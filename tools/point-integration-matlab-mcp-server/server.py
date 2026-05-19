"""
server.py - MCP server for constitutive point-integration reference checks.
"""

from __future__ import annotations

import os
from typing import Any

import yaml
from mcp.server.fastmcp import FastMCP

from matlab_runner import matlab_available, run_generated_j2
from reference_j2 import integrate_j2_strain_path, uniaxial_strain_path


CONFIG_FILE = os.path.join(os.path.dirname(__file__), "config.yaml")
with open(CONFIG_FILE, "r", encoding="utf-8") as file:
    config = yaml.safe_load(file) or {}

MATLAB_EXECUTABLE = config.get("matlab_executable", "matlab")
DEFAULTS = config.get("defaults", {})

mcp = FastMCP("PointIntegrationMatlabServer")


def _parameters(E: float, nu: float, sigma_y0: float, H_iso: float = 0.0) -> dict[str, float]:
    return {
        "E": float(E),
        "nu": float(nu),
        "sigma_y0": float(sigma_y0),
        "H_iso": float(H_iso),
    }


@mcp.tool()
def check_matlab_available() -> dict[str, Any]:
    """检查 MATLAB 可执行文件是否可用。"""
    return matlab_available(MATLAB_EXECUTABLE)


@mcp.tool()
def run_j2_uniaxial_path(
    strain_values: list[float],
    E: float,
    nu: float,
    sigma_y0: float,
    H_iso: float = 0.0,
    prefer_matlab: bool = True,
    allow_python_fallback: bool = True,
) -> dict[str, Any]:
    """
    对 xx 单轴总应变路径执行 J2 线性各向同性硬化单点积分。

    strain_values 是 total strain 序列，例如 [0, 0.001, 0.002]。
    Voigt 顺序为 [xx, yy, zz, xy, yz, zx]，剪切分量采用 tensor strain。
    """
    strain_path = uniaxial_strain_path(strain_values)
    return run_j2_strain_path(
        strain_path=strain_path,
        parameters=_parameters(E, nu, sigma_y0, H_iso),
        prefer_matlab=prefer_matlab,
        allow_python_fallback=allow_python_fallback,
    )


@mcp.tool()
def run_j2_strain_path(
    strain_path: list[list[float]],
    parameters: dict[str, Any],
    prefer_matlab: bool = True,
    allow_python_fallback: bool = True,
) -> dict[str, Any]:
    """
    对 6 分量 total strain path 执行 J2 线性各向同性硬化单点积分。

    parameters 至少包含 E、nu、sigma_y0，可选 H_iso。
    """
    timeout_seconds = int(DEFAULTS.get("timeout_seconds", 120))
    keep_temp_files = bool(DEFAULTS.get("keep_temp_files", False))

    if prefer_matlab:
        matlab_result = run_generated_j2(
            matlab_executable=MATLAB_EXECUTABLE,
            strain_path=strain_path,
            parameters=parameters,
            timeout_seconds=timeout_seconds,
            keep_temp_files=keep_temp_files,
        )
        if matlab_result.get("success"):
            return matlab_result
        if not allow_python_fallback:
            return matlab_result

    result = integrate_j2_strain_path(strain_path, parameters)
    result["backend"] = "python-reference-fallback"
    if prefer_matlab:
        result["warning"] = "MATLAB was requested but unavailable or failed; used Python fallback."
    return result


if __name__ == "__main__":
    mcp.run()
