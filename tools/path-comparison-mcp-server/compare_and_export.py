import os
import numpy as np
import pyvista as pv
import pandas as pd
import matplotlib.pyplot as plt
from scipy.interpolate import interp1d
from result_parser import ResultParser

def compare_grpd_and_ansys(
    vtk_file: str, 
    ansys_txt_file: str, 
    output_dir: str, 
    line_start: tuple = (0.5, 0.0, 0.0), 
    line_end: tuple = (0.5, 5.0, 0.0),
    resolution: int = 100
):
    """
    自动融合 GRPD 的 VTK 结果与 ANSYS 的 TXT 结果，生成 Excel 对比报告和 PNG 曲线图。
    """
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)

    print(f"1. 正在读取 ANSYS 数据: {ansys_txt_file}")
    ansys_data = ResultParser.parse_prvar_output(ansys_txt_file)
    if 'data' not in ansys_data:
        raise ValueError("未能从 ANSYS 结果中提取到有效数据。")
    
    # ANSYS 数据格式约定：[Y, UY, SEQV]
    raw_ansys = np.array(ansys_data['data'])
    ansys_y = raw_ansys[:, 0]
    ansys_uy = raw_ansys[:, 1]
    ansys_seqv = raw_ansys[:, 2]

    print(f"2. 正在读取 GRPD 数据: {vtk_file}")
    mesh = pv.read(vtk_file)
    
    # GRPD 往往输出无拓扑的点云 (Particles)，无法直接用 pyvista.sample 进行插值。
    # 我们采用直接过滤坐标的方式提取特定路径上的粒子。
    pts = mesh.points
    tol = 0.03  # 搜索容差 (假设粒子间距 ~0.05)
    
    target_x = line_start[0]
    mask = (np.abs(pts[:, 0] - target_x) < tol) & \
           (pts[:, 1] >= min(line_start[1], line_end[1]) - tol) & \
           (pts[:, 1] <= max(line_start[1], line_end[1]) + tol)
           
    if np.sum(mask) == 0:
        raise ValueError(f"在 X={target_x} 的路径上未能找到任何粒子！请检查坐标。")
        
    grpd_y = pts[mask, 1]
    grpd_uy = mesh.point_data['Displacement'][mask, 1]
    grpd_seqv = mesh.point_data['VonMisesStress'][mask]

    # 必须对坐标进行排序，否则 interp1d 会报错，且 plot 出来的线是乱码
    sort_idx = np.argsort(grpd_y)
    grpd_y = grpd_y[sort_idx]
    grpd_uy = grpd_uy[sort_idx]
    grpd_seqv = grpd_seqv[sort_idx]

    # 剔除重复的 Y 坐标（极小容差），防止 interp1d 报除零错误
    unique_idx = np.concatenate(([True], np.diff(grpd_y) > 1e-6))
    grpd_y = grpd_y[unique_idx]
    grpd_uy = grpd_uy[unique_idx]
    grpd_seqv = grpd_seqv[unique_idx]

    print("3. 正在进行空间插值与误差计算...")
    # 对 GRPD 数据进行插值，使其与 ANSYS 的坐标对齐
    f_uy = interp1d(grpd_y, grpd_uy, kind='linear', fill_value='extrapolate')
    f_seqv = interp1d(grpd_y, grpd_seqv, kind='linear', fill_value='extrapolate')

    grpd_uy_aligned = f_uy(ansys_y)
    grpd_seqv_aligned = f_seqv(ansys_y)

    # 计算误差
    error_uy = np.abs(grpd_uy_aligned - ansys_uy)
    error_seqv = np.abs(grpd_seqv_aligned - ansys_seqv)
    # 相对误差 (对于位移，使用全局最大值归一化，避免固定端 0.0 位移导致误差爆炸；对于应力，使用点对点误差)
    max_ansys_uy = np.max(np.abs(ansys_uy))
    if max_ansys_uy > 1e-5:
        rel_error_uy = error_uy / max_ansys_uy * 100.0
    else:
        rel_error_uy = error_uy / (np.abs(ansys_uy) + 1e-8) * 100.0
    rel_error_seqv = error_seqv / (np.abs(ansys_seqv) + 1e-8) * 100.0

    print("4. 正在导出 Excel 对比报告...")
    df = pd.DataFrame({
        'Y Coordinate (mm)': ansys_y,
        'ANSYS UY (mm)': ansys_uy,
        'GRPD UY (mm)': grpd_uy_aligned,
        'Error UY (%)': rel_error_uy,
        'ANSYS SEQV (MPa)': ansys_seqv,
        'GRPD SEQV (MPa)': grpd_seqv_aligned,
        'Error SEQV (%)': rel_error_seqv
    })
    excel_path = os.path.join(output_dir, "Comparison_Report.xlsx")
    df.to_excel(excel_path, index=False)

    print("5. 正在生成高保真对比曲线图...")
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(12, 5))

    # 绘制 UY 曲线
    ax1.plot(grpd_y, grpd_uy, 'r-', linewidth=2, label='GRPD (Peridynamics)')
    ax1.plot(ansys_y, ansys_uy, 'ko', markersize=5, label='ANSYS (FEM)')
    ax1.set_xlabel('Y Coordinate (mm)')
    ax1.set_ylabel('Displacement UY (mm)')
    ax1.set_title('Displacement Comparison')
    ax1.legend()
    ax1.grid(True, linestyle='--')

    # 绘制 SEQV 曲线
    ax2.plot(grpd_y, grpd_seqv, 'b-', linewidth=2, label='GRPD (Peridynamics)')
    ax2.plot(ansys_y, ansys_seqv, 'ko', markersize=5, label='ANSYS (FEM)')
    ax2.set_xlabel('Y Coordinate (mm)')
    ax2.set_ylabel('Von Mises Stress (MPa)')
    ax2.set_title('Von Mises Stress Comparison')
    ax2.legend()
    ax2.grid(True, linestyle='--')

    plt.tight_layout()
    plot_path = os.path.join(output_dir, "Comparison_Plot.png")
    plt.savefig(plot_path, dpi=300)
    plt.close()

    max_error_uy = float(np.max(rel_error_uy))
    max_error_seqv = float(np.max(rel_error_seqv))

    # 生成 summary JSON
    import json
    summary_data = {
        "success": True,
        "vtk_file": vtk_file,
        "ansys_txt_file": ansys_txt_file,
        "excel_path": excel_path,
        "plot_path": plot_path,
        "max_error_uy_percent": max_error_uy,
        "max_error_seqv_percent": max_error_seqv
    }
    summary_path = os.path.join(output_dir, "Comparison_Summary.json")
    with open(summary_path, "w", encoding="utf-8") as f:
        json.dump(summary_data, f, indent=4, ensure_ascii=False)

    # 生成 zip 打包文件
    import zipfile
    zip_path = os.path.join(output_dir, "Comparison_Output.zip")
    with zipfile.ZipFile(zip_path, "w") as zipf:
        zipf.write(excel_path, os.path.basename(excel_path))
        zipf.write(plot_path, os.path.basename(plot_path))
        zipf.write(summary_path, os.path.basename(summary_path))

    return {
        "success": True,
        "excel_path": excel_path,
        "plot_path": plot_path,
        "summary_path": summary_path,
        "zip_path": zip_path,
        "max_error_uy_percent": max_error_uy,
        "max_error_seqv_percent": max_error_seqv
    }

if __name__ == "__main__":
    # 作为脚本直接运行时执行测试
    res = compare_grpd_and_ansys(
        vtk_file=r"D:\Project_C++\GRPD\Examples\Axisymmetric_Ring\Result_20260518_090026\Axisymmetric_Ring_step00006_t0.3000.vtk",
        ansys_txt_file=r"D:\ANSYS_Project\GRPD_AXIS\ansys_val_results.txt",
        output_dir=r"D:\ANSYS_Project\GRPD_AXIS",
        line_start=(0.5, 0.0, 0.0),
        line_end=(0.5, 5.0, 0.0)
    )
    print("生成完毕！")
    print(res)
