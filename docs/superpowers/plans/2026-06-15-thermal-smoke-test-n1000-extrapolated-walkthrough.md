# run_0013 算例重跑与线性外插对标测试总结 (Walkthrough)

## 1. 测试概述
为了获得更加严谨和精确的 GRPD 与 ANSYS 仿真对比结果，我们在对标后处理程序 `grpd-validation-mcp-server` 中实现了 **1D 线性外插算法**。
本次测试我们复用了 `run_0013` 在 $t=4.0\text{ s}$ 的物理参数与求解结果（$K_{xx}=1.0$, $\rho=1.0$, $C=1.0$），在 ANSYS 侧进行了二阶 Crank-Nicolson 细密子步求解（$dt=0.01\text{ s}$），并启用边界处的线性外插进行误差对标。

* **测试环境**：Windows (分支: `feature/cross-platform`)
* **算例目录**：[Thermal_Validation_5x10](file:///d:/C++pro/GRPD/Examples/Thermal_Validation_5x10)
* **对标时刻**：$t=4.0\text{ s}$
* **采样路径**：$(0.0, 0.0, 0.0) \to (4.0, 8.0, 0.0)$
* **后处理插值算法**：内部点采用 `IDW` 空间内插，几何边界（投影区外侧）自动切向 `1D 线性外插`。

---

## 2. 误差对标演进对比
随着算法和后处理的逐步修正，我们见证了本算例误差的显著下降：

* **第一阶段 (原 `run_0013`)**: **14.22%** 最大误差
  * *原因*：ANSYS 子步极粗（$dt=1.0\text{ s}$），无 CN 二阶积分导致时间耗散严重；边界采用 IDW 内插导致削平温升极值。
* **第二阶段 (子步加密 + CN隐式)**: **6.24%** 最大误差
  * *原因*：ANSYS 步长缩短 100 倍且使用二阶 CN，时间耗散基本消除。但边界仍使用 IDW，无法外推边界峰值。
* **第三阶段 (子步加密 + CN隐式 + 线性外插)**: **4.25%** 最大误差
  * *原因*：边界采用线性外插（由 $x=0.05$ 和 $x=0.15$ 处粒子外推到 $x=0.0$ 边界），边界温度 GRPD 预测值由 **59.0K 提升至 60.3K**，更加逼近 ANSYS 的真实物理边界值 **62.8K**。
* **测试结论**: **误差从最初的 14.22% 大幅降至 4.25%**，满足对标一致性验证！

---

## 3. 文件及附件链接
* **GRPD 结果 VTK (复用)**: [Thermal_Validation_5x10_step08000_t4.0000.vtk](file:///d:/C++pro/GRPD/Examples/Thermal_Validation_5x10/Result_20260613_004423/Thermal_Validation_5x10_step08000_t4.0000.vtk)
* **加密及外插后对标 Excel 报告**: [Comparison_Report.xlsx](file:///D:/C++pro/GRPD/.agents/mcp/grpd-validation-mcp-server/work_dir/run_0035/Comparison_Report.xlsx)
* **加密及外插后对比曲线图 (Plot)**: [Comparison_Plot.png](file:///D:/C++pro/GRPD/.agents/mcp/grpd-validation-mcp-server/work_dir/run_0035/Comparison_Plot.png)
* **对标打包 ZIP 包**: [Comparison_Output.zip](file:///D:/C++pro/GRPD/.agents/mcp/grpd-validation-mcp-server/work_dir/run_0035/Comparison_Output.zip)
