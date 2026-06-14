# 恢复历史瞬态热传导算例设置对标验证总结 (Walkthrough)

## 1. 算例恢复与配置说明
本算例用于验证在恢复钢材（Steel）物性参数以及多级载荷步（KBC=0 线性 + KBC=1 稳定阶跃）后，GRPD 与 ANSYS 在 $t=4.0\text{ s}$ 时刻的对标精度。
* **物理模型尺寸**: $5.0 \times 10.0\text{ mm}$ (厚度 $1.0\text{ mm}$)，$dx=0.1\text{ mm}$ (5000 粒子/单元)。
* **边界条件**:
  * 左端面 ($X=0$): 施加恒定热流密度 $HFLUX = 100.0$
  * 右端面 ($X=5$): 固定温度 $T = 100.0$
* **求解设置 (GRPD)**:
  * 显式时间步长 $dt = 0.0001\text{ s}$。
  * 载荷步 1: $0.0\text{ s} \le t \le 1.0\text{ s}$，`KBC: 0` (斜坡线性爬升)，细分 $10$ 步 (实际计算 $10000$ 步)。
  * 载荷步 2: $1.0\text{ s} < t \le 10.0\text{ s}$，`KBC: 1` (恒定阶跃)，细分 $90$ 步 (实际计算 $90000$ 步)。
  * 数据提取点：第 $40000$ 步，对应物理时间为真正的 $t = 4.0\text{ s}$。
* **求解设置 (ANSYS)**:
  * 瞬态热传导分析，在 $t = 4.0\text{ s}$ 时刻提取温度解。
* **数据采样路径**: 跨越几何对角线，从 $(0.0, 0.0, 0.0)$ 到 $(4.0, 8.0, 0.0)$。

---

## 2. 求解与验证过程

### GRPD 求解器运行
* **运行状态**: 成功完成 100000 步显式求解。
* **输出 VTK 结果文件**: [Thermal_Validation_5x10_step40000_t4.0000.vtk](file:///d:/C++pro/GRPD/Examples/Thermal_Validation_5x10/Result_20260613_120518/Thermal_Validation_5x10_step40000_t4.0000.vtk)
* **GRPD 求解总耗时**: $164.86\text{ s}$

### ANSYS 求解器运行
* **运行状态**: 成功完成两步双级载荷瞬态热传导求解。
* **输出采样结果**: [ansys_val_results_t4.0.txt](file:///D:/C++pro/GRPD/.agents/mcp/ansys-mcp-server/work_dir/run_0058/ansys_val_results_t4.0.txt)
* **ANSYS 求解总耗时**: $21.44\text{ s}$
* **ANSYS 数据库文件**: [ansys_smoke_test.db](file:///D:/C++pro/GRPD/.agents/mcp/ansys-mcp-server/work_dir/run_0058/ansys_smoke_test.db)（已保留供人工复查）

---

## 3. 对标误差分析 (Validation Results)
通过 `grpd-validation-server` 进行插值及误差计算，在对角采样路径上温度的对标情况如下：
* **最大温度相对误差 (`max_error_temp_percent`)**: **0.31%** (远低于 1.0% 的容差极限)
* **对标分析输出成果**:
  * **详细数据 Excel 报告**: [Comparison_Report.xlsx](file:///D:/C++pro/GRPD/.agents/mcp/grpd-validation-mcp-server/work_dir/run_0014/Comparison_Report.xlsx)
  * **对比分布图**: [Comparison_Plot.png](file:///D:/C++pro/GRPD/.agents/mcp/grpd-validation-mcp-server/work_dir/run_0014/Comparison_Plot.png)

> [!TIP]
> 误差从之前的 **14.22%** 急剧降低到了 **0.31%**，这充分印证了：
> 1. 时间步对标时刻错配问题（原 yaml 运行第 8000 步只有 $t=0.8\text{ s}$）被纠正，GPRD 与 ANSYS 精确在 $t=4.0\text{ s}$ 历史时刻对齐。
> 2. 钢材的高导热性质（导热时间常数 $\tau \approx 2.0\text{ s}$）使得工件在 $t=4.0\text{ s}$ 时已进入热扩散的平缓段，消除了剧烈边界瞬态波锋处的边界粒子截断效应对近场动力学的精度干扰。
> 3. `KBC = 1` 载荷因子的锁定使得第 2 阶段加载完美处于阶跃满载状态。

---

## 4. 实验数据库入库 (Experiment Database Logging)
本次对标仿真结果已成功归档到 GRPD 实验管理系统中：
* **实验记录 ID (`run_id`)**: `1a31c8c0-07a1-4d91-88bf-cda58a42dfce`
* **归档参数**: 几何长宽厚、dx 间距、材料热传导属性（Density, Conductivity, HeatCapacity）、边界条件、采样路径。
* **归档指标**: GRPD 计算时间（164.86s）、ANSYS 求解时间（21.44s）、总步数（100000步）、最大温度相对误差（0.31%）。
* **归档附件**: 已绑定计算 VTK 结果、对标曲线 Comparison_Plot.png 以及 Excel 数据报告。

---

## 5. 对标结论
根据本次三段式管线的验证数据：
**[PASS]** 本次热传导 t=4.0s 时刻冒烟测试成功通过！GRPD 近场动力学热学核心与 ANSYS 物理契合度极佳。
