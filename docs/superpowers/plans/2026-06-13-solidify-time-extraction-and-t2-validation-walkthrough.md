# 固化 TIME 结果提取与 t=2.0s 水平中线对标验证总结 (Walkthrough)

## 1. 固化设计修改说明
为了彻底解决基于子步数 (substep) 提取时极易因隐式/显式迭代步长不一致导致的结果错配问题，我们在 APDL 发生器代码层面进行了逻辑固化：
* **修改文件**: [generator.py](file:///d:/C++pro/GRPD/.agents/mcp/ansys-mcp-server/src/generator.py#L68-L73)
* **实现逻辑**: 在发生器入口，若检测到传入的 `result_time <= 0.0` 但 `result_substep > 0`，会自动根据 YAML 配置中的全局 `TimeStep_dt` 计算出物理时刻 `result_time = result_substep * dt`。
* **效果**: 使得 ANSYS 的后处理始终可以通过物理时间 `TIME`（即 `SET, , , , , TARGET_TIME`）来提取对应的物理场数据，彻底杜绝了使用 GRPD 细密离散步数引起的结果失真与读取报错。

---

## 2. 算例与水平中线 t=2.0s 对标配置
本算例利用恢复钢材物性后的计算历史，在中线路径及 $t=2.0\text{ s}$ 物理时刻进行 GRPD 与 ANSYS 的精度对标。
* **物理模型与网格**: 与前一步完全一致 ($dx=0.1\text{ mm}$)。
* **数据提取点**:
  * **GRPD**: 定位至已算好的第 `20000` 步，对应物理时间为真正的 $t = 2.0\text{ s}$。
  * **ANSYS**: 后处理阶段指定通过物理时间 `TIME = 2.0` 进行结果检索。
* **数据采样路径**: 经典中轴水平采样线，从 $(0.0, 5.0, 0.0)$ 到 $(5.0, 5.0, 0.0)$，总长度为 $5.0\text{ mm}$。

---

## 3. 对标误差分析 (Validation Results)
通过 `grpd-validation-server` 进行插值及误差计算，在水平中线采样路径上温度的对标情况如下：
* **最大温度相对误差 (`max_error_temp_percent`)**: **0.99%** (完美符合一维瞬态传热中线对标)
* **对标分析输出成果**:
  * **详细数据 Excel 报告**: [Comparison_Report.xlsx](file:///D:/C++pro/GRPD/.agents/mcp/grpd-validation-mcp-server/work_dir/run_0016/Comparison_Report.xlsx)
  * **对比分布图**: [Comparison_Plot.png](file:///D:/C++pro/GRPD/.agents/mcp/grpd-validation-mcp-server/work_dir/run_0016/Comparison_Plot.png)

> [!NOTE]
> 在 $t=2.0\text{ s}$ 物理时刻（系统传热的瞬态发展过程中期），水平中线上的最大温度相对误差依然控制在 **0.99%**，证明了 GRPD 在高温度梯度非稳态阶段同样具有极佳的计算精度。

---

## 4. 实验数据库入库 (Experiment Database Logging)
本次对标仿真结果已成功归档到 GRPD 实验管理系统中：
* **实验记录 ID (`run_id`)**: `abace091-1e25-4b7b-b318-1a2b686628e6`
* **归档参数**: 几何长宽厚、dx 间距、材料热传导属性（Density, Conductivity, HeatCapacity）、边界条件、采样中线路径。
* **归档指标**: GRPD 计算时间（164.73s）、ANSYS 求解时间（17.38s）、总步数（100000步）、最大温度相对误差（0.99%）。
* **归档附件**: 已绑定计算 VTK 结果、对比图 Comparison_Plot.png 以及 Excel 数据报告。

---

## 5. 对标结论
根据本次三段式管线的验证数据：
**[PASS]** 本次热传导 t=2.0s 水平中线温度对标验证成功通过！
