# 物理时刻 t=5.0s 中线温度场对标验证总结 (Walkthrough)

## 1. 算例与配置说明
本算例用于验证在恢复钢材物性与多级载荷步后，GRPD 与 ANSYS 在中线路径及 $t=5.0\text{ s}$ 物理时刻的经典热传导精度对标。
* **物理模型尺寸**: $5.0 \times 10.0\text{ mm}$ (厚度 $1.0\text{ mm}$)，$dx=0.1\text{ mm}$ (5000 粒子/单元)。
* **边界条件**:
  * 左端面 ($X=0$): 施加恒定热流密度 $HFLUX = 100.0$
  * 右端面 ($X=5$): 固定温度 $T = 100.0$
* **数据提取点**:
  * **GRPD**: 由于 $dt = 0.0001\text{ s}$，提取第 `50000` 步，对应物理时间为真正的 $t = 5.0\text{ s}$。
  * **ANSYS**: 在瞬态分析中指定检索时间 `TIME = 5.0`。
* **数据采样路径**: 经典中轴水平采样线，从 $(0.0, 5.0, 0.0)$ 到 $(5.0, 5.0, 0.0)$，总长度为 $5.0\text{ mm}$。

---

## 2. 求解与验证过程

### GRPD 求解器运行
* **运行状态**: 定位至已完成的 100000 步显式求解第 50000 步。
* **输出 VTK 结果文件**: [Thermal_Validation_5x10_step50000_t5.0000.vtk](file:///d:/C++pro/GRPD/Examples/Thermal_Validation_5x10/Result_20260613_121325/Thermal_Validation_5x10_step50000_t5.0000.vtk)
* **GRPD 求解总耗时**: $169.71\text{ s}$

### ANSYS 求解器运行
* **运行状态**: 成功完成瞬态求解，后处理通过 `TIME = 5.0` 检索。
* **输出采样结果**: [ansys_val_results_t5.0.txt](file:///D:/C++pro/GRPD/.agents/mcp/ansys-mcp-server/work_dir/run_0059/ansys_val_results_t5.0.txt)
* **ANSYS 求解总耗时**: $17.08\text{ s}$

---

## 3. 对标误差分析 (Validation Results)
通过 `grpd-validation-server` 进行插值及误差计算，在水平中线采样路径上温度的对标情况如下：
* **最大温度相对误差 (`max_error_temp_percent`)**: **0.11%** (完美符合一维导热精细对标图)
* **对标分析输出成果**:
  * **详细数据 Excel 报告**: [Comparison_Report.xlsx](file:///D:/C++pro/GRPD/.agents/mcp/grpd-validation-mcp-server/work_dir/run_0015/Comparison_Report.xlsx)
  * **对比分布图**: [Comparison_Plot.png](file:///D:/C++pro/GRPD/.agents/mcp/grpd-validation-mcp-server/work_dir/run_0015/Comparison_Plot.png)

> [!NOTE]
> 在中轴线路径 $(0,5,0) \to (5,5,0)$ 上，横向热流的对称性消除了 2D 边界非局部粒子截断在横向的干扰，完美展现了 GRPD 的态基近场动力学在一维传热上的求解精度。对比误差低至 **0.11%**。

---

## 4. 实验数据库入库 (Experiment Database Logging)
本次对标仿真结果已成功归档到 GRPD 实验管理系统中：
* **实验记录 ID (`run_id`)**: `d3d6fd7a-1045-4b53-8109-aabe26a84e04`
* **归档参数**: 几何长宽厚、dx 间距、材料热传导属性（Density, Conductivity, HeatCapacity）、边界条件、采样中线路径。
* **归档指标**: GRPD 计算时间（169.71s）、ANSYS 求解时间（17.08s）、总步数（100000步）、最大温度相对误差（0.11%）。
* **归档附件**: 已绑定计算 VTK 结果、对比图 Comparison_Plot.png 以及 Excel 数据报告。

---

## 5. 对标结论
根据本次三段式管线的验证数据：
**[PASS]** 本次热传导 t=5.0s 中轴水平线温度对标验证成功通过！
