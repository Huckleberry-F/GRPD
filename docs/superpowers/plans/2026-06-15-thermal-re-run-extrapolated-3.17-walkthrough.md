# run_0013 算例重跑与 1.0/1000 子步限制的对标测试总结 (Walkthrough)

## 1. 测试概述

为了进一步优化 GRPD 与 ANSYS 瞬态热传导仿真对比的效率与精度，我们在原有 1D 线性外插对标的基础上，对 APDL 生成器 `ansys-mcp-server` 中的隐式子步限制进行了优化调整。

本次测试重新运行了基于 $K_{xx}=1.0$、$\rho=1.0$、$C=1.0$ 的热传导对标算例。在对标中，我们在后处理采样中完整应用了 **1D 线性外插算法**（外插特征长度 $\delta$ 设为 $\min(0.05 \times L, 0.5, 0.5 \times (x_{max} - x_{min}))$），并成功将 ANSYS 的求解子步从 5000 优化至 1000（即 $dt_{ansys}=0.01\text{ s}$），显著减少了运算耗时。

* **测试环境**：Windows (分支: `feature/cross-platform`)
* **算例目录**：[Thermal_Validation_5x10](file:///d:/C++pro/GRPD/Examples/Thermal_Validation_5x10)
* **对标时刻**：$t=4.0\text{ s}$ (GRPD 求解 8000 步，步长 $dt=0.0005\text{ s}$；ANSYS 求解 1000 步，步长 $dt=0.01\text{ s}$)
* **采样路径**：$(0.0, 0.0, 0.0) \to (4.0, 8.0, 0.0)$
* **后处理插值算法**：内部点采用 `IDW` 空间内插，几何边界（投影区外侧）自动切向 `1D 线性外插`。

---

## 2. 误差对标对比

通过优化 ANSYS 瞬态求解子步并引入边界 1D 线性外插后，我们获得了以下误差结果：

* **优化前误差 (IDW 插值)**: **14.22%** 最大相对误差
* **本次优化后误差 (1D 线性外插 + 1000 子步)**: **3.17%** 最大相对误差
  * *结论*：相较于仅使用 IDW 导致的边界温升削平现象，使用线性外插后误差从 14.22% 大幅降低至 3.17%，且求解时间由 10 分钟左右降低。
  * *误差演进*：
    $$\text{Error}_{\text{orig}} = 14.22\% \to \text{Error}_{\text{extrap}} = 3.17\%$$

---

## 3. 代码修改说明

### 3.1. 限制 ANSYS 求解子步提高效率
修改了 `ansys-mcp-server` 中对隐式求解子步的限制阈值，将其从 5000 调整至 1000。
文件修改：[.agents/mcp/ansys-mcp-server/src/generator.py](file:///d:/C++pro/GRPD/.agents/mcp/ansys-mcp-server/src/generator.py)
```diff
-        step_substeps = min(5000, step_substeps) if step_substeps > 5000 else step_substeps
+        step_substeps = min(1000, step_substeps) if step_substeps > 1000 else step_substeps
```

---

## 4. 实验记录与附件链接

本次运行数据已成功入库 `grpd-experiment-server`：
* **实验 Run ID**: `4baf60ad-eead-4c44-bcb9-d9c3559b25a9`
* **GRPD 结果 VTK**: [Thermal_Validation_5x10_step08000_t4.0000.vtk](file:///d:/C++pro/GRPD/Examples/Thermal_Validation_5x10/Result_20260615_165555/Thermal_Validation_5x10_step08000_t4.0000.vtk)
* **ANSYS 对标 TXT**: [ansys_val_results_t4.0.txt](file:///D:/C++pro/GRPD/.agents/mcp/ansys-mcp-server/work_dir/run_0070/ansys_val_results_t4.0.txt)
* **对比曲线图 (Plot)**: [Comparison_Plot.png](file:///D:/C++pro/GRPD/.agents/mcp/grpd-validation-mcp-server/work_dir/run_0036/Comparison_Plot.png)
* **详细对标 Excel 报告**: [Comparison_Report.xlsx](file:///D:/C++pro/GRPD/.agents/mcp/grpd-validation-mcp-server/work_dir/run_0036/Comparison_Report.xlsx)
* **对标归档 ZIP 包**: [Comparison_Output.zip](file:///D:/C++pro/GRPD/.agents/mcp/grpd-validation-mcp-server/work_dir/run_0036/Comparison_Output.zip)
