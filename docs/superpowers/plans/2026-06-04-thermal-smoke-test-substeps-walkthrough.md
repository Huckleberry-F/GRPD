# 2026-06-04 热传导冒烟测试指定子步数与 y=5.0 路径对标总结 (Walkthrough)

## 成果摘要

1. **测试目的与参数变更**：
   - 目的：验证在 `PD.yaml` 中显式指定子步数 `NumSubsteps` 时，对标验证机制中 ANSYS 求解器的计算行为以及对标曲线的精确度。
   - YAML 参数修改：
     - `- Step: 1` 阶段：添加 `NumSubsteps: 10`
     - `- Step: 2` 阶段：添加 `NumSubsteps: 90`
   - 采样路径修改：由斜线对标修改为沿着几何中心剖线（$y = 5.0\text{ mm}$ 恒定），即从 $(0.0, 5.0)$ 采样至 $(5.0, 5.0)$。采样路径物理长度为 $5.0\text{ mm}$，处于 $5.0\text{ mm} \times 10.0\text{ mm}$ 的几何区域中央。

2. **ANSYS 子步数与对标对齐结果**：
   - APDL 脚本生成器正确地将 YAML 文件中指定的子步数转换到了 APDL 脚本中：第一加载步使用 `NSUBST, 10, 10, 10`，第二加载步使用 `NSUBST, 90, 90, 90`。
   - 提取对标时间：物理时刻 $t = 5.0\text{ s}$。
   - **温度场 (TEMP) 对标结果**：最大相对误差为 **$0.115\%$** (远小于 $0.2\%$ 的高精度判定界限) $\rightarrow$ **[PASS]**。这证明了在引入自定义子步数后，ANSYS 得到的数值解与 GRPD 显式积分（$dt=0.0001$，运行 $50000$ 步）的温度场分布吻合度极高。

---

## 自动化工具流执行细节与记录对照

### 物理时间 $t = 5.0\text{ s}$ 对标运行
- **GRPD VTK 结果提取**：调用 `grpd-server.get_grpd_vtk_result` (指定 `substep=50000`)
  - 最新运行输出目录：`d:/Project_C++/GRPD/Examples/Thermal_Validation_MCP/Result_20260604_152125`
  - 提取的 VTK 文件：`Result_step50000_t5.0000.vtk`
  - GRPD 求解器运行耗时：`201.92 s`
- **ANSYS 求解与文本提取**：调用 `ansys-server.run_ansys_yaml_case` (指定 `time=5.0`, 采样线 `y=5.0` 的起点与终点)
  - 隔离沙箱工作目录：`D:\Project_C++\GRPD\.agents\mcp\ansys-mcp-server\work_dir\run_0055`
  - 提取的数据结果文件：`ansys_val_results_t5.0.txt`
  - ANSYS 求解与后处理提取耗时：`14.26 s`
- **数据切片融合与误差对比**：调用 `grpd-validation-server.compare_grpd_vtk_with_ansys_txt` (物理类型为 `"Thermal"`)
  - 自动运行对齐和 K-近邻反距离空间加权插值 (IDW)。
  - 对比输出工作目录：`d:\Project_C++\GRPD\.agents\mcp\grpd-validation-mcp-server\work_dir\run_0011`
  - **最大温度相对误差**：`0.11477%`
- **中央实验追踪库归档 (Run ID)**: `1fce9588-0a56-4dea-95d2-a1de1ee1a663`
  - 绑定资产 (Excel 报告)：[Comparison_Report.xlsx](file:///d:/Project_C++/GRPD/.agents/mcp/grpd-validation-mcp-server/work_dir/run_0011/Comparison_Report.xlsx)
  - 绑定资产 (对标图表)：[Comparison_Plot.png](file:///d:/Project_C++/GRPD/.agents/mcp/grpd-validation-mcp-server/work_dir/run_0011/Comparison_Plot.png)
  - 绑定资产 (摘要 JSON)：[Comparison_Summary.json](file:///d:/Project_C++/GRPD/.agents/mcp/grpd-validation-mcp-server/work_dir/run_0011/Comparison_Summary.json)
