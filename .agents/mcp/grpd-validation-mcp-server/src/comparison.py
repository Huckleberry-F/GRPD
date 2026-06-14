from __future__ import annotations

import json
import os
import zipfile

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import pyvista as pv

from .result_parser import ResultParser


FIELD_MAPPING = {
    "UX": ("Displacement", 0, "Displacement UX", "mm"),
    "UY": ("Displacement", 1, "Displacement UY", "mm"),
    "UZ": ("Displacement", 2, "Displacement UZ", "mm"),
    "SEQV": ("VonMisesStress", None, "Von Mises Stress", "MPa"),
    "TEMP": ("Temperature", None, "Temperature", "K"),
}


def _default_components(physics_type: str) -> list[str]:
    return ["TEMP"] if physics_type.lower() == "thermal" else ["UY", "SEQV"]


def _ansys_column_index(component: str, physics_type: str, column_count: int) -> int:
    if physics_type.lower() == "thermal":
        candidate = 1
    else:
        candidate = 1 if component == "UY" else 2
    if candidate >= column_count:
        raise ValueError(f"ANSYS 导出的列数不足，无法定位 {component} 列")
    return candidate


def compare_grpd_and_ansys(
    vtk_file: str,
    ansys_txt_file: str,
    output_dir: str,
    line_start: tuple[float, float, float] = (0.5, 0.0, 0.0),
    line_end: tuple[float, float, float] = (0.5, 5.0, 0.0),
    physics_type: str = "Mechanical",
    components: list[str] | None = None,
    tol: float | None = None,
    x_axis_mode: str = "auto",
) -> dict:
    """Compare GRPD VTK point data with ANSYS text results along a 3D sampling line."""
    os.makedirs(output_dir, exist_ok=True)

    ansys_data = ResultParser.parse_prvar_output(ansys_txt_file)
    if "data" not in ansys_data:
        raise ValueError("未能从 ANSYS 结果中提取到有效数据！")

    raw_ansys = np.array(ansys_data["data"], dtype=float)
    ansys_dist = raw_ansys[:, 0]
    components = components or _default_components(physics_type)

    mesh = pv.read(vtk_file)
    pts = mesh.points

    p_start = np.array(line_start, dtype=float)
    p_end = np.array(line_end, dtype=float)
    v_line = p_end - p_start
    line_length = float(np.linalg.norm(v_line))
    if line_length < 1.0e-6:
        raise ValueError("采样直线长度接近为0，请提供有效的起始点与终止点坐标。")

    u_line = v_line / line_length
    v_pts = pts - p_start
    d_proj = np.dot(v_pts, u_line)
    v_perp = v_pts - np.outer(d_proj, u_line)
    d_perp = np.linalg.norm(v_perp, axis=1)

    if tol is None:
        in_range = (d_proj >= 0.0) & (d_proj <= line_length)
        tol = max(float(np.min(d_perp[in_range])) * 1.5, 0.03) if np.any(in_range) else 0.03

    mask = (d_perp < tol) & (d_proj >= -tol) & (d_proj <= line_length + tol)
    if not np.any(mask):
        raise ValueError("在指定的采样路径附近未过滤出任何粒子！请检查几何坐标范围。")

    grpd_proj_dist = d_proj[mask]
    sort_idx = np.argsort(grpd_proj_dist)
    grpd_proj_dist_sorted = grpd_proj_dist[sort_idx]
    unique_idx = np.concatenate(([True], np.diff(grpd_proj_dist_sorted) > 1.0e-6))
    grpd_proj_dist_final = grpd_proj_dist_sorted[unique_idx]

    excel_data = {"Distance (mm)": ansys_dist}
    summary_data = {
        "success": True,
        "vtk_file": os.path.abspath(vtk_file),
        "ansys_txt_file": os.path.abspath(ansys_txt_file),
    }
    plot_data = {}

    for component in components:
        if component not in FIELD_MAPPING:
            raise ValueError(f"不受支持的物理量分量: {component}")

        field_name, field_dim, label, unit = FIELD_MAPPING[component]
        if field_name not in mesh.point_data:
            raise ValueError(f"GRPD VTK 中不包含该物理场: {field_name}")

        grpd_raw = mesh.point_data[field_name][mask]
        grpd_values_filtered = grpd_raw[:, field_dim] if field_dim is not None else grpd_raw

        # 计算每一个 ANSYS 采样点在真实 3D 空间中的坐标
        ansys_coords = p_start + np.outer(ansys_dist, u_line)  # shape: (M, 3)

        # 过滤后的 GRPD 粒子坐标
        grpd_pts_filtered = pts[mask]  # shape: (N, 3)

        # 计算距离矩阵 dists shape: (M, N)
        diff = ansys_coords[:, np.newaxis, :] - grpd_pts_filtered[np.newaxis, :, :]
        dists = np.linalg.norm(diff, axis=2)

        # 对每个采样点，取最近的 K 个邻近粒子进行空间反距离加权插值 (IDW)
        M, N = dists.shape
        K = min(4, N)
        grpd_aligned = np.zeros(M)
        for j in range(M):
            idx = np.argsort(dists[j])[:K]
            d = dists[j, idx]
            val = grpd_values_filtered[idx]
            w = 1.0 / (d + 1e-6) ** 2
            grpd_aligned[j] = np.sum(w * val) / np.sum(w)

        ansys_col_idx = _ansys_column_index(component, physics_type, raw_ansys.shape[1])
        ansys_values = raw_ansys[:, ansys_col_idx]

        abs_err = np.abs(grpd_aligned - ansys_values)
        max_ansys = float(np.max(np.abs(ansys_values)))
        if max_ansys > 1.0e-5:
            rel_err = abs_err / max_ansys * 100.0
        else:
            rel_err = abs_err / (np.abs(ansys_values) + 1.0e-8) * 100.0

        excel_data[f"ANSYS {component} ({unit})"] = ansys_values
        excel_data[f"GRPD {component} ({unit})"] = grpd_aligned
        excel_data[f"Error {component} (%)"] = rel_err

        summary_data[f"max_error_{component.lower()}_percent"] = float(np.max(rel_err))
        ansys_x_coords = ansys_coords[:, 0]
        ansys_y_coords = ansys_coords[:, 1]
        ansys_z_coords = ansys_coords[:, 2]

        mode = x_axis_mode.lower() if x_axis_mode else "auto"
        if mode == "x":
            plot_x = ansys_x_coords
            x_label = "X coordinate (mm)"
        elif mode == "y":
            plot_x = ansys_y_coords
            x_label = "Y coordinate (mm)"
        elif mode == "z":
            plot_x = ansys_z_coords
            x_label = "Z coordinate (mm)"
        elif mode == "distance":
            plot_x = ansys_dist
            x_label = "Distance along path (mm)"
        else:  # "auto"
            if (np.max(ansys_x_coords) - np.min(ansys_x_coords)) > 1.0e-5:
                plot_x = ansys_x_coords
                x_label = "X coordinate (mm)"
            elif (np.max(ansys_y_coords) - np.min(ansys_y_coords)) > 1.0e-5:
                plot_x = ansys_y_coords
                x_label = "Y coordinate (mm)"
            elif (np.max(ansys_z_coords) - np.min(ansys_z_coords)) > 1.0e-5:
                plot_x = ansys_z_coords
                x_label = "Z coordinate (mm)"
            else:
                plot_x = ansys_dist
                x_label = "Distance along path (mm)"

        plot_data[component] = (
            plot_x,
            grpd_aligned,
            plot_x,
            ansys_values,
            label,
            unit,
            x_label,
        )

    excel_path = os.path.join(output_dir, "Comparison_Report.xlsx")
    pd.DataFrame(excel_data).to_excel(excel_path, index=False)

    fig, axes = plt.subplots(1, len(components), figsize=(6 * len(components), 5))
    if len(components) == 1:
        axes = [axes]
    for ax, component in zip(axes, components):
        gr_x, gr_y, an_x, an_y, label, unit, x_label = plot_data[component]
        ax.plot(gr_x, gr_y, "r-", linewidth=2, label="GRPD (Peridynamics)")
        ax.plot(an_x, an_y, "ko", markersize=5, label="ANSYS (FEM)")
        ax.set_xlabel(x_label)
        ax.set_ylabel(f"{label} ({unit})")
        ax.set_title(f"{component} Comparison")
        ax.legend()
        ax.grid(True, linestyle="--")

    plt.tight_layout()
    plot_path = os.path.join(output_dir, "Comparison_Plot.png")
    plt.savefig(plot_path, dpi=300)
    plt.close(fig)

    summary_path = os.path.join(output_dir, "Comparison_Summary.json")
    with open(summary_path, "w", encoding="utf-8") as file:
        json.dump(summary_data, file, indent=4, ensure_ascii=False)

    zip_path = os.path.join(output_dir, "Comparison_Output.zip")
    with zipfile.ZipFile(zip_path, "w") as archive:
        archive.write(excel_path, os.path.basename(excel_path))
        archive.write(plot_path, os.path.basename(plot_path))
        archive.write(summary_path, os.path.basename(summary_path))

    result = {
        "success": True,
        "excel_path": excel_path,
        "plot_path": plot_path,
        "summary_path": summary_path,
        "zip_path": zip_path,
    }
    for component in components:
        result[f"max_error_{component.lower()}_percent"] = summary_data[
            f"max_error_{component.lower()}_percent"
        ]
    return result
