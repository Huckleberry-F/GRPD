# -*- coding: utf-8 -*-
import os
import sys
from typing import Any

import yaml


def _first_load_step_substeps(solver: dict[str, Any], fallback: int = 20) -> int:
    load_steps = solver.get("LoadSteps", [])
    if load_steps:
        return int(load_steps[0].get("NumSubsteps", fallback))
    return fallback


def generate_apdl_from_yaml(
    yaml_path: str,
    output_mac_path: str,
    *,
    job_name: str = "ansys_smoke_test",
    result_substep: int = 0,
    result_time: float = 0.0,
    default_start_x: float | None = None,
    default_end_x: float | None = None,
    default_start_y: float | None = None,
    default_end_y: float | None = None,
) -> dict[str, Any]:
    print(f"[APDL Generator] Loading YAML: {yaml_path}", file=sys.stderr)
    with open(yaml_path, "r", encoding="utf-8") as file:
        data = yaml.safe_load(file) or {}

    parts = data.get("Parts", [])
    if not parts:
        raise ValueError("No Parts defined in YAML.")
    part = parts[0]

    materials = data.get("Materials", [])
    if not materials:
        raise ValueError("No Materials defined in YAML.")
    material = materials[0]

    solver = data.get("Solver", {})
    solver_type = solver.get("Type", "Mechanical")
    kernel = solver.get("Kernel", "")
    integrator = solver.get("TimeIntegrator", "Explicit")
    num_substeps = _first_load_step_substeps(solver)

    dim = int(part.get("Dimension", 2))
    is_axis = "Axis" in kernel
    length = float(part.get("Length", 2.0))
    width = float(part.get("Width", 5.0))
    height = float(part.get("Height", 1.0))
    offset = part.get("Offset", [0.0, 0.0, 0.0])
    dx = float(part.get("dx", 0.05))

    x1 = float(offset[0])
    x2 = x1 + length
    y1 = float(offset[1])
    y2 = y1 + width
    z1 = float(offset[2]) if len(offset) >= 3 else 0.0
    z2 = z1 + height

    fallback_start_x = x1 if default_start_x is None else float(default_start_x)
    fallback_end_x = fallback_start_x if default_end_x is None else float(default_end_x)
    fallback_start_y = y1 if default_start_y is None else float(default_start_y)
    fallback_end_y = y2 if default_end_y is None else float(default_end_y)

    if result_time <= 0.0 and result_substep > 0:
        dt = float(solver.get("TimeStep_dt", 0.0))
        if dt > 0.0:
            result_time = result_substep * dt

    if result_time > 0.0:
        output_stem = f"ansys_val_results_t{result_time}"
    else:
        output_stem = f"ansys_val_results_sub{result_substep}" if result_substep else "ansys_val_results"

    ey = float(material.get("YoungsModulus", 200000.0))
    prxy = float(material.get("PoissonsRatio", 0.3))
    yield_stress = float(material.get("YieldStress", 400.0))
    hardening = float(material.get("LinearHardening", 0.0))

    thermal_k = float(material.get("Conductivity", 45.0))
    thermal_c = float(material.get("HeatCapacity", 4.6e8))
    thermal_rho = float(material.get("Density", 7.85e-9))

    is_thermal = solver_type.lower() == "thermal"

    apdl: list[str] = [
        "!==============================================================================",
        "! 自动生成的 ANSYS APDL 验证文件",
        "!==============================================================================",
        "FINISH",
        "/CLEAR, NOSTART",
        "/PREP7",
        "",
    ]

    if dim == 3:
        if is_thermal:
            apdl += ["! 三维热学实体单元配置 (SOLID70)", "ET, 1, 70"]
        else:
            apdl += ["! 三维实体单元配置 (SOLID185)", "ET, 1, 185"]
    elif is_axis:
        if is_thermal:
            apdl += ["! 轴对称热学单元配置 (PLANE55)", "ET, 1, 55", "KEYOPT, 1, 3, 1"]
        else:
            apdl += ["! 轴对称单元配置 (PLANE182)", "ET, 1, 182", "KEYOPT, 1, 3, 1"]
    else:
        if is_thermal:
            apdl += ["! 普通2D面单元热学配置 (PLANE55 - 平面)", "ET, 1, 55", "KEYOPT, 1, 3, 0"]
        else:
            apdl += ["! 普通2D面单元配置 (PLANE182 - 平面应变)", "ET, 1, 182", "KEYOPT, 1, 3, 2"]

    apdl += [""]
    if is_thermal:
        apdl += [
            f"MP, KXX, 1, {thermal_k}",
            f"MP, DENS, 1, {thermal_rho}",
            f"MP, C, 1, {thermal_c}",
        ]
    else:
        apdl += [
            f"MP, EX, 1, {ey}",
            f"MP, PRXY, 1, {prxy}",
        ]

        if "J2" in str(material.get("Type", "")):
            et = (ey * hardening) / (ey + hardening) if hardening > 0 else 0.0
            apdl += [
                "! J2 isotropic hardening elastoplastic material",
                "TB, BISO, 1, 1, 2",
                f"TBDATA, 1, {yield_stress}, {et}",
            ]

    apdl.append("")
    if dim == 3:
        apdl += [
            "! 建立三维实体几何",
            f"BLOCK, {x1}, {x2}, {y1}, {y2}, {z1}, {z2}",
            f"ESIZE, {dx}",
            "MSHAPE, 0, 3D",
            "MSHKEY, 1",
            "VMESH, ALL",
        ]
    else:
        apdl += [
            "! 建立二维截面几何",
            f"RECTNG, {x1}, {x2}, {y1}, {y2}",
            f"ESIZE, {dx}",
            "MSHAPE, 0, 2D",
            "MSHKEY, 1",
            "AMESH, ALL",
        ]

    apdl += ["", "! 施加常驻边界条件"]
    for bc in data.get("BoundaryConditions", []):
        _append_bc(apdl, bc, dim, is_thermal)

    apdl += ["", "! 施加多级载荷条件"]
    for step_cond in data.get("LoadStep_Conditions", []):
        for bc in step_cond.get("BoundaryConditions", []):
            _append_bc(apdl, bc, dim, is_thermal)

    # 识别分析类型是否为瞬态
    is_thermal = solver_type.lower() == "thermal"
    is_transient = False
    if is_thermal:
        if integrator.lower() in ["explicit", "implicit"]:
            is_transient = True
    else:
        if integrator.lower() in ["explicit"]:
            is_transient = True

    apdl += [
        "/SOLU",
    ]

    if is_transient:
        apdl += [
            "ANTYPE, TRANS",      # 瞬态分析
            "TIMINT, ON",         # 开启时间积分
        ]
        if is_thermal:
            apdl += [
                "TINTP, , 0.5",   # 启用 Crank-Nicolson 二阶无阻尼积分
            ]
    else:
        apdl += [
            "ANTYPE, STATIC",     # 静力学/稳态分析
        ]

    apdl += [
        "NLGEOM, ON" if material.get("LargeDeformation", False) else "NLGEOM, OFF",
        "OUTRES, ALL, ALL",
    ]

    # 多步载荷步求解
    load_steps = solver.get("LoadSteps", [])
    if not load_steps:
        load_steps = [{"Step": 1, "Time": 1.0, "KBC": 0}]

    for step in load_steps:
        step_time = float(step.get("Time", 1.0))
        step_kbc = int(step.get("KBC", 0))
        step_substeps = int(step.get("NumSubsteps", num_substeps))
        # 限制隐式求解步数，避免跟随 GRPD 显式细密步长
        step_substeps = min(100, step_substeps) if step_substeps > 100 else step_substeps

        if is_transient:
            apdl += [
                f"TIME, {step_time}",
                f"KBC, {step_kbc}",
                f"NSUBST, {step_substeps}, {step_substeps}, {step_substeps}",
                "SOLVE",
            ]
        else:
            apdl += [
                f"KBC, {step_kbc}",
                "SOLVE",
            ]
            break

    apdl += [
        f"SAVE, {job_name}, db",
        "FINISH",
        "",
        "! Automatic post-processing data extraction",
        "/POST1",
        f"SUBSTEP = {int(result_substep)}",
        f"TARGET_TIME = {float(result_time)}",
        f"START_X = {fallback_start_x}",
        f"END_X = {fallback_end_x}",
        f"START_Y = {fallback_start_y}",
        f"END_Y = {fallback_end_y}",
    ]

    if result_time > 0.0:
        apdl.append("SET, , , , , TARGET_TIME")
    else:
        apdl.append("SET, 1, SUBSTEP" if result_substep else "SET, LAST")

    apdl += [
        "",
        "",
        "PATH, MyPath, 2, 30, 10",
    ]

    if dim == 3:
        apdl += [
            f"PPATH, 1, 0, START_X, START_Y, {z1}",
            f"PPATH, 2, 0, END_X, END_Y, {z2}",
        ]
    else:
        apdl += [
            "PPATH, 1, 0, START_X, START_Y, 0.0",
            "PPATH, 2, 0, END_X, END_Y, 0.0",
        ]

    apdl += [""]
    if is_thermal:
        apdl += [
            "PDEF, TEMPVAL, TEMP",
            "",
            f"/OUTPUT, {output_stem}, txt",
            "PRPATH, TEMPVAL",
            "/OUTPUT",
            "",
        ]
    else:
        apdl += [
            "PDEF, UYVAL, U, Y",
            "PDEF, SEQVVAL, S, EQV",
            "",
            f"/OUTPUT, {output_stem}, txt",
            "PRPATH, UYVAL, SEQVVAL",
            "/OUTPUT",
            "",
        ]

    os.makedirs(os.path.dirname(os.path.abspath(output_mac_path)), exist_ok=True)
    with open(output_mac_path, "w", encoding="utf-8") as file:
        file.write("\n".join(apdl))

    ansys_txt_file = os.path.join(os.path.dirname(os.path.abspath(output_mac_path)), f"{output_stem}.txt")
    db_file = os.path.join(os.path.dirname(os.path.abspath(output_mac_path)), f"{job_name}.db")
    print(f"[APDL Generator] Successfully wrote MAC file to {output_mac_path}", file=sys.stderr)
    return {"success": True, "mac_file": output_mac_path, "ansys_txt_file": ansys_txt_file, "db_file": db_file}


def _append_bc(apdl: list[str], bc: dict[str, Any], dim: int, is_thermal: bool) -> None:
    box = bc.get("Box", [])
    bc_type = bc.get("Type", "")
    value = bc.get("Value", 0.0)

    is_flux = is_thermal and bc_type.upper() == "FLUX"

    if is_flux:
        # 根据 Box 的几何跨度推断其边界物理法向，锁定几何边界防止多维溢热
        if dim == 3 and len(box) >= 6:
            dx_box = abs(box[1] - box[0])
            dy_box = abs(box[3] - box[2])
            dz_box = abs(box[5] - box[4])
            min_dim = min(dx_box, dy_box, dz_box)
            if min_dim == dx_box:
                apdl += [
                    f"ASEL, S, LOC, X, {box[0]}, {box[1]}",
                    f"SFA, ALL, 1, HFLUX, {value}",
                    "ASEL, ALL",
                ]
            elif min_dim == dy_box:
                apdl += [
                    f"ASEL, S, LOC, Y, {box[2]}, {box[3]}",
                    f"SFA, ALL, 1, HFLUX, {value}",
                    "ASEL, ALL",
                ]
            else:
                apdl += [
                    f"ASEL, S, LOC, Z, {box[4]}, {box[5]}",
                    f"SFA, ALL, 1, HFLUX, {value}",
                    "ASEL, ALL",
                ]
        else:  # 2D 问题
            dx_box = abs(box[1] - box[0])
            dy_box = abs(box[3] - box[2])
            if dx_box < dy_box:
                apdl += [
                    f"LSEL, S, LOC, X, {box[0]}, {box[1]}",
                    f"SFL, ALL, HFLUX, {value}",
                    "LSEL, ALL",
                ]
            else:
                apdl += [
                    f"LSEL, S, LOC, Y, {box[2]}, {box[3]}",
                    f"SFL, ALL, HFLUX, {value}",
                    "LSEL, ALL",
                ]
    else:
        if len(box) >= 6 and dim == 3:
            apdl += [
                f"NSEL, S, LOC, X, {box[0]}, {box[1]}",
                f"NSEL, R, LOC, Y, {box[2]}, {box[3]}",
                f"NSEL, R, LOC, Z, {box[4]}, {box[5]}",
            ]
        elif len(box) >= 4:
            apdl += [
                f"NSEL, S, LOC, X, {box[0]}, {box[1]}",
                f"NSEL, R, LOC, Y, {box[2]}, {box[3]}",
            ]

        if is_thermal:
            if bc_type.upper() in {"T", "TEMP"}:
                apdl.append(f"D, ALL, TEMP, {value}")
        else:
            if bc_type in {"UX", "UY"} or (bc_type == "UZ" and dim == 3):
                apdl.append(f"D, ALL, {bc_type}, {value}")
        apdl += ["ALLSEL, ALL", ""]
