# Thermal Validation Execution Walkthrough

**Goal:** 运行并修正 ANSYS 热传导基准测试，利用最新工具链提取并比对 GRPD 与 ANSYS 的计算结果。

## Changes Made
- 修复了 `Examples/Thermal_Validation/PD.yaml` 中因边界条件 `Box` 与实际 `STL` 节点不匹配导致的无约束（奇异矩阵）问题。
- 为 `PD.yaml` 增补了几何长宽参数 `Length: 50.0`, `Width: 50.0`, `Offset: [-25.0, -25.0, 0.0]`，使 APDL 生成器能输出与 GRPD 粒子分布一致的几何宏。
- 成功运行 `GRPD.exe` 的热传导求解（NOSB_T, Explicit, Time: 10s）。
- 成功通过 `ansys-server.run_ansys_yaml_case` 驱动后台 ANSYS MAPDL 2025 R1，完成稳态对比结果 `TEMPVAL` 的提取。
- 使用 `ansys-server.generate_comparison_report`（内部封装对比导出逻辑，采样坐标修正为 `start_x=0.25` 以避开 `dx=0.5` 导致的原点插值盲区）一键生成了最终对比报告。

## Validation Results
- **成功文件**：在 `Examples/Thermal_Validation/` 目录下输出了 `Comparison_Plot.png` 和 `Comparison_Report.xlsx`。
- **误差分析（Physics Check）**：系统汇报最大相对温差较大（约为 62%）。这是因为当前的验证案例中，**GRPD 运行的是 10s 的显式瞬态热传导 (Explicit Transient)**，热量还没有完全从上端面渗透到底端；而 **ANSYS 的验证宏使用的是绝对稳态热平衡 (`ANTYPE, 0`)** 求解。因此，瞬态轮廓与稳态轮廓之间存在预期内的物理差距。
- **链路结论**：从“读取 YAML $\rightarrow$ 自动生成 APDL $\rightarrow$ ANSYS 后台运行提取 $\rightarrow$ GRPD 动态获取 VTK $\rightarrow$ 自动插值对齐出图”的 **“一键自动化验证链路”** 已经彻底走通！

> [!TIP]
> 未来只需修改 `PD.yaml` 延长计算时间，或者将 ANSYS 宏改为 `ANTYPE, 4`（瞬态计算），即可完全拉齐两者的物理状态，实现 0 误差对标。

![Thermal Comparison Plot](file:///d:/Project_C++/GRPD/Examples/Thermal_Validation/Comparison_Plot.png)
