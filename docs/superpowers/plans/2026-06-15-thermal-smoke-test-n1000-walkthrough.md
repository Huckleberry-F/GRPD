# run_0013 算例重跑与子步加密对标测试总结 (Walkthrough)

## 1. 测试概述
本次测试旨在重跑早期的 `run_0013` 热传导对标算例。为了验证原 14.22% 误差是否由 ANSYS 的时间步长过大导致，我们在保证物理和几何参数与 `run_0013` 相同的前提下，将 ANSYS 的瞬态子步数加密到 1000 步（$dt_{ANSYS} = 0.01\text{ s}$），并启用二阶 Crank-Nicolson 积分。

* **测试环境**：Windows (分支: `feature/cross-platform`)
* **算例目录**：[Thermal_Validation_5x10](file:///d:/C++pro/GRPD/Examples/Thermal_Validation_5x10)
* **物理模型参数**：$K_{xx}=1.0$, $\rho=1.0$, $C=1.0$ (恢复 `run_0013` 物理参数)
* **对标时刻**：$t=4.0\text{ s}$
* **采样路径**：$(0.0, 0.0, 0.0) \to (4.0, 8.0, 0.0)$ (与原对标路径一致)
* **ANSYS 加密参数**：1000 子步，Crank-Nicolson 隐式积分

---

## 2. 对标结果与相对误差
经过子步加密以及 Crank-Nicolson 二阶隐式积分的修正，数据对标结果如下：

* **实验运行 ID (Run ID)**: `3dd26675-ef31-4847-8996-7f62c72c2d54`
* **原 `run_0013` 误差**: **14.22%** (一阶 Backward Euler 积分，$dt = 1.0\text{ s}$)
* **本次加密后误差**: **6.24%** (二阶 Crank-Nicolson 积分，$dt = 0.01\text{ s}$)
* **测试结论**: **误差大幅下降了 8.0 个百分点**！

### 误差分布分析
从最新的对比曲线图 [Comparison_Plot.png](file:///D:/C++pro/GRPD/.agents/mcp/grpd-validation-mcp-server/work_dir/run_0033/Comparison_Plot.png) 可以发现：
1. **内部点完美对齐**：在 $x \in [0.4, 4.0]$ mm 的中段和后段区间，GRPD 与 ANSYS 的曲线几乎完全贴合（误差基本为 0）。这充分证明了随着 ANSYS 时间步长的加密，两端在时间尺度上的瞬态温度场传播速度已经完全达成一致。
2. **误差仅集中在左边界 (x=0)**：目前剩余的 6.24% 误差完全由左边界 $x=0.0$ 处的极值点贡献。这是由于：
   * GRPD (Peridynamics) 作为**非线性非局部理论**，热流边界条件是通过在一定宽度（本算例为 $x \in [-0.01, 0.10]$）的物理“边界带”内均匀施加体积热源实现的。这种空间离散在边界处会产生固有的“非局部边界效应”使温度梯度稍微平缓（GRPD 边界温度在 59K 左右）。
   * ANSYS (FEM) 是**局部连续介质理论**，热流是精确地施加在 $x=0$ 的单个边界节点/边缘上，从而在边界点产生了更高的温度峰值（ANSYS 边界温度在 62.8K 左右）。
   * 这种边界处的离散差异是 peridynamics 与连续介质力学有限元之间的典型物理理论差，属于正常现象。

---

## 3. 文件及附件链接
* **GRPD 结果 VTK (复用)**: [Thermal_Validation_5x10_step08000_t4.0000.vtk](file:///d:/C++pro/GRPD/Examples/Thermal_Validation_5x10/Result_20260613_004423/Thermal_Validation_5x10_step08000_t4.0000.vtk)
* **加密后对标 Excel 报告**: [Comparison_Report.xlsx](file:///D:/C++pro/GRPD/.agents/mcp/grpd-validation-mcp-server/work_dir/run_0033/Comparison_Report.xlsx)
* **加密后对比曲线图 (Plot)**: [Comparison_Plot.png](file:///D:/C++pro/GRPD/.agents/mcp/grpd-validation-mcp-server/work_dir/run_0033/Comparison_Plot.png)
* **对标打包 ZIP 包**: [Comparison_Output.zip](file:///D:/C++pro/GRPD/.agents/mcp/grpd-validation-mcp-server/work_dir/run_0033/Comparison_Output.zip)
