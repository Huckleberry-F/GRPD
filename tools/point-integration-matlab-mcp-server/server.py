"""
server.py - MCP server for constitutive point-integration reference checks using MATLAB.
"""

from __future__ import annotations

import os
from typing import Any

import yaml
from mcp.server.fastmcp import FastMCP

from matlab_runner import matlab_available, run_dynamic_constitutive

CONFIG_FILE = os.path.join(os.path.dirname(__file__), "config.yaml")
with open(CONFIG_FILE, "r", encoding="utf-8") as file:
    config = yaml.safe_load(file) or {}

MATLAB_EXECUTABLE = config.get("matlab_executable", "matlab")
DEFAULTS = config.get("defaults", {})

mcp = FastMCP("PointIntegrationMatlabServer")


def uniaxial_strain_path(strain_values: list[float]) -> list[list[float]]:
    """
    将单轴拉伸应变值列表转换为六分量 Voigt 应变路径。
    GRPD Voigt 顺序为 [xx, yy, zz, xy, yz, zx]，剪切分量采用张量应变。
    """
    return [[float(e), 0.0, 0.0, 0.0, 0.0, 0.0] for e in strain_values]


@mcp.tool()
def check_matlab_available() -> dict[str, Any]:
    """检查 MATLAB 可执行文件是否可用。"""
    return matlab_available(MATLAB_EXECUTABLE)


@mcp.tool()
def run_constitutive_path(
    strain_path: list[list[float]],
    material_type: str,
    parameters: dict[str, Any],
    work_dir: str = "",
) -> dict[str, Any]:
    """
    对指定的 6 分量应变路径执行本构单点积分（完全使用 MATLAB 求解，无 Python 备用）。
    
    参数:
    - strain_path: 应变历史，每一项为 [xx, yy, zz, xy, yz, zx]，其中剪切分量采用张量应变。
    - material_type: 材料本构类型，如 "J2LinearIsotropicHardening" 或 "J2VoceLemaitre"。
    - parameters: 材料参数字典。
      - 对 "J2LinearIsotropicHardening": E, nu, sigma_y0, H_iso (可选，默认 0.0)
      - 对 "J2VoceLemaitre": YoungsModulus, PoissonsRatio, YieldStress, LinearHardening, 
        VoceR, VoceB, LemaitreS, Lemaitre_s, DamageThreshold, CriticalDamage, 
        DamageAccelThreshold, DamageAccelFactor
    - work_dir: 可选，查验工作目录。若指定，则在该目录下生成脚本和结果文件且不自动删除，并打包为 ZIP。
    """
    timeout_seconds = int(DEFAULTS.get("timeout_seconds", 120))
    keep_temp_files = bool(DEFAULTS.get("keep_temp_files", False))

    return run_dynamic_constitutive(
        matlab_executable=MATLAB_EXECUTABLE,
        strain_path=strain_path,
        material_type=material_type,
        parameters=parameters,
        work_dir=work_dir,
        timeout_seconds=timeout_seconds,
        keep_temp_files=keep_temp_files,
    )


@mcp.tool()
def run_uniaxial_constitutive_path(
    strain_values: list[float],
    material_type: str,
    parameters: dict[str, Any],
    work_dir: str = "",
) -> dict[str, Any]:
    """
    对单轴总应变路径执行本构单点积分。
    
    参数:
    - strain_values: 单轴应变值序列，例如 [0.0, 0.001, 0.002, 0.003]。
    - material_type: 材料本构类型，如 "J2LinearIsotropicHardening" 或 "J2VoceLemaitre"。
    - parameters: 材料参数字典。
    - work_dir: 可选，查验工作目录。若指定，则在该目录下生成脚本和结果文件且不自动删除，并打包为 ZIP。
    """
    strain_path = uniaxial_strain_path(strain_values)
    return run_constitutive_path(
        strain_path=strain_path,
        material_type=material_type,
        parameters=parameters,
        work_dir=work_dir,
    )


if __name__ == "__main__":
    mcp.run()
