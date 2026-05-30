import os
import numpy as np
import pyvista as pv
import pandas as pd
from compare_and_export import compare_grpd_and_ansys

def test_3d_diagonal_comparison():
    # 1. 模拟生成一条沿三维斜对角线的粒子点云 (从 0,0,0 到 10,10,10)
    n_points = 50
    t = np.linspace(0, 10, n_points)
    x = t
    y = t
    z = t
    points = np.column_stack((x, y, z))
    
    # 加上少许扰动以测试容差过滤
    points += np.random.normal(0, 0.01, points.shape)
    
    mesh = pv.PolyData(points)
    # 模拟位移 (UX, UY, UZ) 和温度 (TEMP)
    mesh.point_data['Displacement'] = np.column_stack((t*0.1, t*0.2, t*0.3))
    mesh.point_data['VonMisesStress'] = t * 10.0
    mesh.point_data['Temperature'] = 300.0 + t * 5.0
    
    vtk_path = "mock_mesh.vtk"
    mesh.save(vtk_path)
    
    # 2. 模拟生成对应的 ANSYS 导出 txt 文件 (采样局部坐标从 0 到 L=17.32)
    L = np.sqrt(300.0) # 10*sqrt(3)
    ansys_t = np.linspace(0, L, 20)
    # 线性插值生成对应的理论位移 UY 和应力 SEQV
    ansys_uy = (ansys_t / L * 10) * 0.2
    ansys_seqv = (ansys_t / L * 10) * 10.0
    
    ansys_txt_path = "mock_ansys.txt"
    with open(ansys_txt_path, "w") as f:
        f.write("      DIST            UY          SEQV\n")
        for d, uy, seqv in zip(ansys_t, ansys_uy, ansys_seqv):
            f.write(f" {d:12.6f} {uy:12.6f} {seqv:12.6f}\n")
            
    # 3. 运行三维斜线采样对比 (从 0,0,0 到 10,10,10)
    output_dir = "test_output"
    try:
        res = compare_grpd_and_ansys(
            vtk_file=vtk_path,
            ansys_txt_file=ansys_txt_path,
            output_dir=output_dir,
            line_start=(0.0, 0.0, 0.0),
            line_end=(10.0, 10.0, 10.0),
            physics_type="Mechanical",
            components=["UY", "SEQV"],
            tol=0.1
        )
        assert res["success"] is True
        assert "max_error_uy_percent" in res
        assert "max_error_seqv_percent" in res
        assert os.path.exists(os.path.join(output_dir, "Comparison_Report.xlsx"))
        assert os.path.exists(os.path.join(output_dir, "Comparison_Plot.png"))
    finally:
        # 清理临时文件
        for path in [vtk_path, ansys_txt_path]:
            if os.path.exists(path):
                os.remove(path)
        import shutil
        if os.path.exists(output_dir):
            shutil.rmtree(output_dir)

if __name__ == "__main__":
    test_3d_diagonal_comparison()
    print("All mock tests passed!")
