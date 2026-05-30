import os
import numpy as np
import pyvista as pv
import pandas as pd
import matplotlib.pyplot as plt
from scipy.interpolate import interp1d
from result_parser import ResultParser

FIELD_MAPPING = {
    "UX": ("Displacement", 0, "Displacement UX", "mm"),
    "UY": ("Displacement", 1, "Displacement UY", "mm"),
    "UZ": ("Displacement", 2, "Displacement UZ", "mm"),
    "SEQV": ("VonMisesStress", None, "Von Mises Stress", "MPa"),
    "TEMP": ("Temperature", None, "Temperature", "K"),
}

def compare_grpd_and_ansys(
    vtk_file: str,
    ansys_txt_file: str,
    output_dir: str,
    line_start: tuple = (0.5, 0.0, 0.0),
    line_end: tuple = (0.5, 5.0, 0.0),
    resolution: int = 100,
    physics_type: str = "Mechanical",
    components: list = None,
    tol: float = None
):
    """
    自动融合 GRPD 的 VTK 结果与 ANSYS 的 TXT 结果，生成 Excel 对比报告与 PNG 曲线图。
    支持任意三维斜线对角采样、自定义通道过滤与自适应容差。
    """
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)

    print(f"1. 正在读取 ANSYS 数据: {ansys_txt_file}")
    ansys_data = ResultParser.parse_prvar_output(ansys_txt_file)
    if 'data' not in ansys_data:
        raise ValueError("未能从 ANSYS 结果中提取到有效数据！")

    raw_ansys = np.array(ansys_data['data'])
    ansys_dist = raw_ansys[:, 0]  # 第一列通常是沿路径的距离 DIST

    if components is None:
        if physics_type.lower() == "thermal":
            components = ["TEMP"]
        else:
            components = ["UY", "SEQV"]

    print(f"2. 正在读取 GRPD 数据: {vtk_file}")
    mesh = pv.read(vtk_file)
    pts = mesh.points

    # 三维向量投影数学模型
    p_start = np.array(line_start)
    p_end = np.array(line_end)
    v_line = p_end - p_start
    L_line = np.linalg.norm(v_line)
    if L_line < 1e-6:
        raise ValueError("采样直线长度接近为0，请提供有效的起始点与终止点坐标。")
    
    u_line = v_line / L_line  # 单位方向向量

    # 计算所有粒子到起点的向量
    v_pts = pts - p_start
    # 投影距离与垂直正交距离
    d_proj = np.dot(v_pts, u_line)
    v_perp = v_pts - np.outer(d_proj, u_line)
    d_perp = np.linalg.norm(v_perp, axis=1)

    # 自适应容差估计
    if tol is None:
        in_range = (d_proj >= 0) & (d_proj <= L_line)
        if np.sum(in_range) > 0:
            min_perp = np.min(d_perp[in_range])
            tol = max(min_perp * 1.5, 0.03)  # 容差为最小距离的1.5倍，且兜底0.03mm
        else:
            tol = 0.03
    print(f"应用投影过滤容差 tol = {tol:.4f} mm")

    # 过滤掩码
    mask = (d_perp < tol) & (d_proj >= -tol) & (d_proj <= L_line + tol)
    if np.sum(mask) == 0:
        raise ValueError(f"在指定的采样路径附近未过滤出任何粒子！请检查几何坐标范围。")

    grpd_proj_dist = d_proj[mask]
    
    # 遍历待对比物理量通道并计算插值与相对误差
    excel_data = {"Distance (mm)": ansys_dist}
    summary_data = {
        "success": True,
        "vtk_file": vtk_file,
        "ansys_txt_file": ansys_txt_file,
    }
    errors_dict = {}

    # 排序以防乱序点云造成插值失效
    sort_idx = np.argsort(grpd_proj_dist)
    grpd_proj_dist_sorted = grpd_proj_dist[sort_idx]

    # 滤重
    unique_idx = np.concatenate(([True], np.diff(grpd_proj_dist_sorted) > 1e-6))
    grpd_proj_dist_final = grpd_proj_dist_sorted[unique_idx]

    for comp_name in components:
        if comp_name not in FIELD_MAPPING:
            raise ValueError(f"不受支持的物理量分量: {comp_name}")
        
        field_name, field_dim, label, unit = FIELD_MAPPING[comp_name]
        
        # 提取 GRPD 物理场
        if field_name not in mesh.point_data:
            raise ValueError(f"GRPD VTK 中不包含该物理场: {field_name}")
        
        grpd_val_raw = mesh.point_data[field_name][mask]
        if field_dim is not None:
            # 向量数据提取特定分量
            grpd_val = grpd_val_raw[:, field_dim]
        else:
            grpd_val = grpd_val_raw
            
        grpd_val_sorted = grpd_val[sort_idx][unique_idx]

        # 对齐 ANSYS 对应的列数据
        # 根据 component_name 在 ANSYS 列名列表中做匹配
        ansys_col_idx = -1
        for idx, col in enumerate(ansys_data.get('columns', [])):
            if comp_name.upper() in col.upper():
                ansys_col_idx = idx
                break
        if ansys_col_idx == -1:
            # 退位采用传统默认列索引
            if physics_type.lower() == "thermal":
                ansys_col_idx = 1
            else:
                ansys_col_idx = 1 if comp_name == "UY" else 2

        if ansys_col_idx >= raw_ansys.shape[1]:
            raise ValueError(f"ANSYS 导出的列数不足，无法定位 {comp_name} 列")

        ansys_val = raw_ansys[:, ansys_col_idx]

        # 线性插值对齐
        f_interp = interp1d(grpd_proj_dist_final, grpd_val_sorted, kind='linear', fill_value='extrapolate')
        grpd_val_aligned = f_interp(ansys_dist)

        # 误差计算
        abs_err = np.abs(grpd_val_aligned - ansys_val)
        max_ansys_val = np.max(np.abs(ansys_val))
        if max_ansys_val > 1e-5:
            rel_err = abs_err / max_ansys_val * 100.0
        else:
            rel_err = abs_err / (np.abs(ansys_val) + 1e-8) * 100.0

        excel_data[f"ANSYS {comp_name} ({unit})"] = ansys_val
        excel_data[f"GRPD {comp_name} ({unit})"] = grpd_val_aligned
        excel_data[f"Error {comp_name} (%)"] = rel_err

        max_err_pct = float(np.max(rel_err))
        summary_data[f"max_error_{comp_name.lower()}_percent"] = max_err_pct
        errors_dict[comp_name] = (grpd_proj_dist_final, grpd_val_sorted, ansys_dist, ansys_val, label, unit)

    # 导出 Excel
    df = pd.DataFrame(excel_data)
    excel_path = os.path.join(output_dir, "Comparison_Report.xlsx")
    df.to_excel(excel_path, index=False)

    # 绘制多通道高保真对比曲线图
    n_comps = len(components)
    fig, axes = plt.subplots(1, n_comps, figsize=(6 * n_comps, 5))
    if n_comps == 1:
        axes = [axes]

    for ax, comp_name in zip(axes, components):
        gr_x, gr_y, an_x, an_y, label, unit = errors_dict[comp_name]
        ax.plot(gr_x, gr_y, 'r-', linewidth=2, label='GRPD (Peridynamics)')
        ax.plot(an_x, an_y, 'ko', markersize=5, label='ANSYS (FEM)')
        ax.set_xlabel("Distance along path (mm)")
        ax.set_ylabel(f"{label} ({unit})")
        ax.set_title(f"{comp_name} Comparison")
        ax.legend()
        ax.grid(True, linestyle='--')

    plt.tight_layout()
    plot_path = os.path.join(output_dir, "Comparison_Plot.png")
    plt.savefig(plot_path, dpi=300)
    plt.close()

    summary_path = os.path.join(output_dir, "Comparison_Summary.json")
    with open(summary_path, "w", encoding="utf-8") as f:
        import json
        json.dump(summary_data, f, indent=4, ensure_ascii=False)

    # 生成 zip
    import zipfile
    zip_path = os.path.join(output_dir, "Comparison_Output.zip")
    with zipfile.ZipFile(zip_path, "w") as zipf:
        zipf.write(excel_path, os.path.basename(excel_path))
        zipf.write(plot_path, os.path.basename(plot_path))
        zipf.write(summary_path, os.path.basename(summary_path))

    result_dict = {
        "success": True,
        "excel_path": excel_path,
        "plot_path": plot_path,
        "summary_path": summary_path,
        "zip_path": zip_path,
    }
    for comp_name in components:
        result_dict[f"max_error_{comp_name.lower()}_percent"] = summary_data[f"max_error_{comp_name.lower()}_percent"]

    return result_dict

if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser(description="Compare GRPD and ANSYS results")
    parser.add_argument("--vtk", required=True, help="GRPD VTK file path")
    parser.add_argument("--ansys", required=True, help="ANSYS txt result file path")
    parser.add_argument("--output", default=".", help="Output directory")
    parser.add_argument("--type", default="Mechanical", choices=["Mechanical", "Thermal"], help="Physics type")
    parser.add_argument("--start_x", type=float, default=0.5, help="Line start X")
    parser.add_argument("--start_y", type=float, default=0.0, help="Line start Y")
    parser.add_argument("--start_z", type=float, default=0.0, help="Line start Z")
    parser.add_argument("--end_x", type=float, default=0.5, help="Line end X")
    parser.add_argument("--end_y", type=float, default=5.0, help="Line end Y")
    parser.add_argument("--end_z", type=float, default=0.0, help="Line end Z")
    parser.add_argument("--components", nargs="+", default=None, help="Physics components list (e.g. UY SEQV)")
    parser.add_argument("--tol", type=float, default=None, help="Tolerance distance for filtering")
    
    args = parser.parse_args()
    
    res = compare_grpd_and_ansys(
        vtk_file=args.vtk,
        ansys_txt_file=args.ansys,
        output_dir=args.output,
        line_start=(args.start_x, args.start_y, args.start_z),
        line_end=(args.end_x, args.end_y, args.end_z),
        physics_type=args.type,
        components=args.components,
        tol=args.tol
    )
    print("生成完毕。")
    import json
    print(json.dumps(res, indent=4, ensure_ascii=False))
