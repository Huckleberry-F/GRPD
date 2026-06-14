# ANSYS 对标 MCP 服务与验证流程重构实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 重构 `ansys-mcp-server` 和对应的验证 Playbook 技能，融合三维自适应投影算法与多物理场通道配置映射，从而全面修复位移硬编码、斜线采样失败和容差不匹配的缺陷。

**Architecture:** 在 `compare_and_export.py` 中引入三维向量投影数学模型，用投影距离 $d_{proj}$ 统一轴线，使用字段映射字典 `FIELD_MAPPING` 动态解析 GRPD VTK 与 ANSYS 列名并循环生成对比，并在 `server.py` 微服务暴露相关接口。

**Tech Stack:** Python 3.12, PyVista, NumPy, SciPy, Pandas, FastMCP, SQLite

---

### Task 1: 编写 Mock 测试验证脚本
在重构前，我们需要先编写一个单元测试来覆盖新的三维自适应投影与多通道映射算法，确保能利用 PyVista 模拟生成三维粒子网格，不依赖外部真实的 ANSYS 软件即可验证算法正确性。

**Files:**
- Create: `d:/Project_C++/GRPD/.agents/mcp/ansys-mcp-server/test_compare.py`

- [ ] **Step 1: 创建测试文件**
  编写包含三维斜向线段粒子和 ANSYS 数据对准的 Mock 单元测试。
  ```python
  import os
  import numpy as np
  import pyvista as pv
  import pandas as pd
  import pytest
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
  ```

- [ ] **Step 2: 运行测试并确信其失败**
  在重构前运行该测试，由于当前的 `compare_and_export` 不支持 `components` 和斜线投影，必然报错。
  运行：`python d:/Project_C++/GRPD/.agents/mcp/ansys-mcp-server/test_compare.py`
  预期：FAIL 并抛出 TypeError 或 ValueError。

- [ ] **Step 3: 提交初始测试文件**
  ```bash
  git add .agents/mcp/ansys-mcp-server/test_compare.py
  git commit -m "test: add mock test for 3D diagonal and component-mapped alignment"
  ```

---

### Task 2: 重构 [compare_and_export.py](file:///d:/Project_C++/GRPD/.agents/mcp/ansys-mcp-server/compare_and_export.py)
实现三维自适应投影算法和基于 `FIELD_MAPPING` 配置的多通道物理场比对。

**Files:**
- Modify: `d:/Project_C++/GRPD/.agents/mcp/ansys-mcp-server/compare_and_export.py`

- [ ] **Step 1: 实现核心投影逻辑与字段映射**
  替换 `compare_grpd_and_ansys` 中的旧有平行过滤逻辑，并引入多字段支持。
  ```python
  # 修改 d:/Project_C++/GRPD/.agents/mcp/ansys-mcp-server/compare_and_export.py 替换整个 compare_grpd_and_ansys 函数

  FIELD_MAPPING = {
      "UX": ("Displacement", 0, "Displacement UX (mm)", "mm"),
      "UY": ("Displacement", 1, "Displacement UY (mm)", "mm"),
      "UZ": ("Displacement", 2, "Displacement UZ (mm)", "mm"),
      "SEQV": ("VonMisesStress", None, "Von Mises Stress (MPa)", "MPa"),
      "TEMP": ("Temperature", None, "Temperature (K)", "K"),
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

      for comp_idx, comp_name in enumerate(components):
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
  ```

- [ ] **Step 2: 运行测试并确保其通过**
  运行：`python d:/Project_C++/GRPD/.agents/mcp/ansys-mcp-server/test_compare.py`
  预期：PASS，屏幕打印 "All mock tests passed!"。

- [ ] **Step 3: 提交 compare_and_export 变动**
  ```bash
  git add .agents/mcp/ansys-mcp-server/compare_and_export.py
  git commit -m "refactor: implement 3D diagonal vector projection and multi-channel component mapping"
  ```

---

### Task 3: 更新 [server.py](file:///d:/Project_C++/GRPD/.agents/mcp/ansys-mcp-server/server.py) 的 FastMCP 接口
在 MCP 服务的接口层透传这些新参数，并升级 SQLite 数据库的字段扩展，确保与前向版本的兼容。

**Files:**
- Modify: `d:/Project_C++/GRPD/.agents/mcp/ansys-mcp-server/server.py`

- [ ] **Step 1: 升级 generate_comparison_report 的参数签名**
  修改 `server.py` 里的 `generate_comparison_report` 函数签名和文档注释，同时支持透传新参数给底层的 `compare_grpd_and_ansys`：
  ```python
  # 修改 d:/Project_C++/GRPD/.agents/mcp/ansys-mcp-server/server.py 约 293-343 行的 generate_comparison_report

  @mcp.tool()
  def generate_comparison_report(
      vtk_file: str,
      ansys_txt_file: str,
      output_dir: str = "",
      start_x: float = 0.5, start_y: float = 0.0, start_z: float = 0.0,
      end_x: float = 0.5, end_y: float = 5.0, end_z: float = 0.0,
      physics_type: str = "Mechanical",
      components: list = None,
      tol: float = None
  ) -> dict:
      """
      读取 GRPD 求解器输出的 VTK 文件与 ANSYS 的文本结果文件，自适应投影对齐并计算多物理场相对误差。
      支持任意三维斜线对角采样、自定义通道过滤与自适应容差。
      :param vtk_file: GRPD 输出的二进制 VTK 文件的绝对路径。
      :param ansys_txt_file: ANSYS 导出的 TXT 结果文件的绝对路径。
      :param output_dir: 报告保存目录。
      :param start_x, start_y, start_z: 采样直线起始点坐标。
      :param end_x, end_y, end_z: 采样直线终止点坐标。
      :param physics_type: "Mechanical" 或 "Thermal"。
      :param components: 待对标物理场通道列表，如 ["UX", "UY", "SEQV"]。
      :param tol: 粒子过滤的几何垂直距离容差 (mm)，不传则自适应估算。
      """
      if not output_dir:
          base_work_dir = os.path.join(os.path.dirname(__file__), "work_dir")
          output_dir = get_next_work_dir(base_work_dir)
      else:
          output_dir = os.path.abspath(output_dir)

      os.makedirs(output_dir, exist_ok=True)

      line_start = (start_x, start_y, start_z)
      line_end = (end_x, end_y, end_z)

      result = compare_grpd_and_ansys(
          vtk_file=vtk_file,
          ansys_txt_file=ansys_txt_file,
          output_dir=output_dir,
          line_start=line_start,
          line_end=line_end,
          physics_type=physics_type,
          components=components,
          tol=tol
      )

      # 写入 SQLite 数据库
      try:
          save_ansys_comparison_to_db(
              db_path=DB_FILE,
              vtk_file=vtk_file,
              ansys_txt_file=ansys_txt_file,
              max_error_uy_percent=result.get("max_error_uy_percent", 0.0),
              max_error_seqv_percent=result.get("max_error_seqv_percent", 0.0),
              output_dir=output_dir,
              excel_path=result.get("excel_path", ""),
              plot_path=result.get("plot_path", ""),
              zip_path=result.get("zip_path", ""),
              max_error_temp_percent=result.get("max_error_temp_percent", 0.0)
          )
      except Exception as db_err:
          result["db_warning"] = f"写入 SQLite 数据库失败: {str(db_err)}"

      return result
  ```

- [ ] **Step 2: 验证单元测试通过**
  运行：`python d:/Project_C++/GRPD/.agents/mcp/ansys-mcp-server/test_compare.py`
  预期：PASS。

- [ ] **Step 3: 提交 server.py 变动**
  ```bash
  git add .agents/mcp/ansys-mcp-server/server.py
  git commit -m "feat: expose components and adaptive tol parameters on FastMCP wrapper"
  ```

---

### Task 4: 更新 GRPD 冒烟测试 Playbook 与 Skill 定义
将新重构后的 3D 采样、自适应容差以及物理量映射参数更新至冒烟测试的指南和调用示例。

**Files:**
- Modify: `d:/Project_C++/GRPD/.agents/skills/superCAE/skills/grpd-smoke-tester/references/smoke-test-playbook.md`
- Modify: `d:/Project_C++/GRPD/.agents/skills/superCAE/skills/grpd-smoke-tester/SKILL.md`

- [ ] **Step 1: 修改 smoke-test-playbook.md 中的一键脚本调用示例**
  将 `smoke-test-playbook.md` 中的 API 调用示例修正，支持 `start_z`, `end_z`, `components` 和 `tol` 参数。
  ```diff
  -    ansys-server.generate_comparison_report(
  -        vtk_file="<GRPD_VTK>",
  -        ansys_txt_file="<ANSYS_TXT>",
  -        output_dir="<Comparison_Output_Dir>",
  -        start_x=<Start_X_Coordinate>,
  -        end_x=<End_X_Coordinate>,
  -        physics_type="<Mechanical|Thermal>"
  -    )
  +    ansys-server.generate_comparison_report(
  +        vtk_file="<GRPD_VTK>",
  +        ansys_txt_file="<ANSYS_TXT>",
  +        output_dir="<Comparison_Output_Dir>",
  +        start_x=<Start_X_Coordinate>,
  +        start_y=<Start_Y_Coordinate>,
  +        start_z=<Start_Z_Coordinate>,
  +        end_x=<End_X_Coordinate>,
  +        end_y=<End_Y_Coordinate>,
  +        end_z=<End_Z_Coordinate>,
  +        physics_type="<Mechanical|Thermal>",
  +        components=["UX", "UY", "SEQV"],  # 动态指定对标的分量
  +        tol=<Optional_Adaptive_Tol>        # 允许指定过滤宽度
  +    )
  ```

- [ ] **Step 2: 修改 grpd-smoke-tester/SKILL.md**
  同步修改 `SKILL.md` 的必做流程部分，更新接口调用形式以支持新的三维与通道映射参数。

- [ ] **Step 3: 提交 Playbook 与 Skill 的更新**
  ```bash
  git add .agents/skills/superCAE/skills/grpd-smoke-tester/
  git commit -m "docs: update smoke test playbook and skill files for new 3D projection parameters"
  ```
