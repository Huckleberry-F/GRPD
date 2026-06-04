# 2026-06-04 热传导冒烟测试斜线对标与插值算法升级总结 (Walkthrough)

## 成果摘要
1. **测试目的与路径变更**：
   - 运行 GRPD 与 ANSYS 在热传导（Thermal）物理场上的对标验证。
   - 采样路径：由原先平行于坐标轴的路径，修改为从 $(0, 0)$ 到 $(2, 5)$ 的二维斜线采样路径。
   - 采样路径的物理长度公式为：
     $$L = \sqrt{(X_{end} - X_{start})^2 + (Y_{end} - Y_{start})^2} = \sqrt{2^2 + 5^2} = \sqrt{29} \approx 5.3852\text{ mm}$$
   - 算例真实的几何尺寸为 $5.0\text{ mm} \times 10.0\text{ mm}$，斜线采样点完全处于物理几何范围内。

2. **对标数据抽取算法空间化升级 (解决台阶状波动)**：
   - **原算法缺陷**：原先的验证服务器直接将落在直线管道内的离散粒子投影到一维线段上排序并去重，然后直接使用一维 `np.interp` 对标。当路径为斜线时，由于粒子沿正方形网格分布，在垂直于采样线方向存在实际场值偏差。这种一维粗暴点云排序插值导致温度曲线出现剧烈的**“锯齿状跳变”与“台阶状波动”**。
   - **升级后算法**：我们将 [comparison.py](file:///D:/Project_C++/GRPD/.agents/mcp/grpd-validation-mcp-server/src/comparison.py) 中的提取逻辑升级为 **$K$ 近邻空间反距离加权插值 (IDW, Inverse Distance Weighting)**：
     - 计算采样路径上各插值坐标点 $\mathbf{x} = (x, y, z)$ 的三维空间真实坐标：
       $$\mathbf{x}(d) = \mathbf{p}_{start} + d \cdot \mathbf{u}_{line}$$
     - 搜寻过滤粒子中距离 $\mathbf{x}$ 最近的 $K$ 个粒子（$K = 4$）。
     - 计算反距离空间权重并加权平均：
       $$w_i(\mathbf{x}) = \frac{1}{(\|\mathbf{x} - \mathbf{x}_i\| + \epsilon)^2}$$
       $$T(\mathbf{x}) = \frac{\sum_{i=1}^K w_i(\mathbf{x}) T_i}{\sum_{i=1}^K w_i(\mathbf{x})}$$
     - 升级后，插值曲线成功消除了所有锯齿和局部台阶，变得平滑、连续，与 ParaView 连续空间网格插值得出的真实曲线完全一致。

3. **两组物理时间点对标结果**：
   - 物理类型：**热传导 (Thermal)**
   - 待对标物理场：**温度场 (TEMP)**
   - **载荷步 1 末尾 ($t = 5.0\text{ s}$)**：最大相对误差为 **$0.775\%$** (基于空间 IDW) $\rightarrow$ **[PASS]**
   - **载荷步 2 末尾 ($t = 10.0\text{ s}$)**：最大相对误差为 **$0.165\%$** (基于空间 IDW) $\rightarrow$ **[PASS]**

---

## 自动化工具流执行细节与记录对照

### 运行 1：物理时间 $t = 10.0\text{ s}$ (最终收敛步)
- **GRPD VTK 结果提取**：`grpd-server.get_grpd_vtk_result` (指定 `substep=100000`)
  - 生成文件：`d:/Project_C++/GRPD/Examples/Thermal_Validation_MCP/Result_20260604_093347/Result_step100000_t10.0000.vtk`
  - 求解执行耗时：`188.31 s`
- **ANSYS 求解与文本提取**：`ansys-server.run_ansys_yaml_case` (指定 `time=10.0`, `substep=0`)
  - 工作目录：`D:\Project_C++\GRPD\.agents\mcp\ansys-mcp-server\work_dir\run_0053`
  - ANSYS 结果文件：`ansys_val_results_t10.0.txt`
  - 求解执行耗时：`4.87 s`
- **数据切片融合与误差对比**：`grpd-validation-server.compare_grpd_vtk_with_ansys_txt` (基于 IDW 空间平滑插值)
  - 结果输出目录：`d:\Project_C++\GRPD\.agents\mcp\grpd-validation-mcp-server\work_dir\run_010`
- **中央实验追踪库归档 (Run ID)**: `58997b01-3a92-4bb4-95e9-50fb04651757`
  - 绑定资产 (Plot): [Comparison_Plot_t10.0.png](file:///d:/Project_C++/GRPD/.agents/mcp/grpd-validation-mcp-server/work_dir/run_010/Comparison_Plot.png)
  - 绑定资产 (Report): [Comparison_Report_t10.0.xlsx](file:///d:/Project_C++/GRPD/.agents/mcp/grpd-validation-mcp-server/work_dir/run_010/Comparison_Report.xlsx)

### 运行 2：物理时间 $t = 5.0\text{ s}$ (过程步，基于 IDW)
- **GRPD VTK 结果提取**：`grpd-server.get_grpd_vtk_result` (指定 `substep=50000`)
  - 生成文件：`d:/Project_C++/GRPD/Examples/Thermal_Validation_MCP/Result_20260604_141833/Result_step50000_t5.0000.vtk`
  - 求解执行耗时：`155.29 s`
- **ANSYS 求解与文本提取**：`ansys-server.run_ansys_yaml_case` (指定 `time=5.0`, `substep=0`)
  - 工作目录：`D:\Project_C++\GRPD\.agents\mcp\ansys-mcp-server\work_dir\run_0054`
  - ANSYS 结果文件：`ansys_val_results_t5.0.txt`
  - 求解执行耗时：`7.85 s`
- **数据切片融合与误差对比**：`grpd-validation-server.compare_grpd_vtk_with_ansys_txt` (基于 IDW 空间平滑插值)
  - 结果输出目录：`d:\Project_C++\GRPD\.agents\mcp\grpd-validation-mcp-server\work_dir\run_009`
- **中央实验追踪库归档 (Run ID)**: `23ff9437-fffd-4c30-b0d0-91b96575aaaf`
  - 绑定资产 (Plot): [Comparison_Plot_t5.0.png](file:///d:/Project_C++/GRPD/.agents/mcp/grpd-validation-mcp-server/work_dir/run_009/Comparison_Plot.png)
  - 绑定资产 (Report): [Comparison_Report_t5.0.xlsx](file:///d:/Project_C++/GRPD/.agents/mcp/grpd-validation-mcp-server/work_dir/run_009/Comparison_Report.xlsx)
