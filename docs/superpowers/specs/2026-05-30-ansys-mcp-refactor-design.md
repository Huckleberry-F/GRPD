# ANSYS 对标 MCP 服务与验证流程重构设计方案

本项目中，自动化冒烟测试与精度对标强依赖于 `ansys-mcp-server` 微服务及 `grpd-smoke-tester` 技能。为解决当前位移分量硬编码、采样直线必须平行于坐标轴、采样粒子容差不可调以及不支持三维斜线等关键缺陷，本设计方案将“方案 A (三维自适应投影投影算法)”与“方案 B (物理场多通道通用配置映射)”进行深度融合与重构。

## 1. 核心重构目标
1. **支持任意三维斜线采样**：替换当前仅支持平行于 X/Y 轴的一刀切逻辑，采用向量点积投影算法，实现任意空间三维直线采样的物理场提取。
2. **自适应容差与带宽控制**：支持动态传入过滤容差 $tol$。当未指定时，根据采样线附近的粒子分布密度自适应估算特征间距 $dx$，动态给出合适的过滤带宽度。
3. **物理场多通道映射 (方案 B)**：支持在报表生成接口指定待对标的物理场分量（例如 UX, UY, UZ, SEQV, TEMP 等），解除只能硬编码对比 UY 与 VonMisesStress 的限制。
4. **接口及 Playbook 同步更新**：将上述参数暴露在 `ansys-server` 接口，并在 `grpd-smoke-tester` 技能中更新对应的调用示例。

---

## 2. 方案 A 的数学模型与投影算法

设采样直线的起点为 $\mathbf{P}_s = (x_s, y_s, z_s)$，终点为 $\mathbf{P}_e = (x_e, y_e, z_e)$。

### 2.1 坐标投影关系
定义直线的长度为 $L = \|\mathbf{P}_e - \mathbf{P}_s\|$，直线的单位方向向量为：
$$\mathbf{u} = \frac{\mathbf{P}_e - \mathbf{P}_s}{L}$$

对于 GRPD 粒子点云中任意粒子 $\mathbf{X}_i = (x_i, y_i, z_i)$，计算其相对于起点的向量 $\mathbf{v}_i = \mathbf{X}_i - \mathbf{P}_s$。
粒子在采样线上的投影相对距离（局部坐标值）为：
$$d_{proj, i} = \mathbf{v}_i \cdot \mathbf{u}$$

粒子到采样直线的垂直正交距离为：
$$d_{\perp, i} = \sqrt{\|\mathbf{v}_i\|^2 - (d_{proj, i})^2}$$

### 2.2 粒子过滤掩码 (Filter Mask)
为了仅筛选出靠近采样线且在起终点范围内的粒子，过滤条件掩码定义为：
$$d_{\perp, i} < tol \quad \text{and} \quad -tol \le d_{proj, i} \le L + tol$$

### 2.3 自适应容差估计
若调用方未指定 $tol$，算法将通过以下方式自适应估算：
1. 取采样线中轴点处的局部范围，计算附近粒子的平均间距 $\overline{dx}$。
2. 设定 $tol = 0.5 \cdot \overline{dx}$（或 $0.6 \cdot \overline{dx}$），确保每个投影截面上都有且仅有最邻近的一排粒子落入掩码区，从而消除多层重叠粒子的干扰。

---

## 3. 方案 B 的物理场通用映射设计

为解除硬编码，我们将对标的物理场拆解为配置化的映射关系。

### 3.1 对标配置定义
在对标报告中，允许指定对标的物理量通道 `components`，格式为字符串列表，例如 `["UY", "SEQV"]`。
每一通道对应三个关键属性：
1. **GRPD VTK 中的 Point Data 数组名及分量索引**（如：`Displacement` 的 Y 分量索引为 1）。
2. **ANSYS 导出的文本结果中的列索引/列名**。
3. **输出图表的物理单位和轴标签**。

默认映射关系定义为：
* **Mechanical (力学场)**:
  * `UY` -> GRPD: `Displacement` [分量 1]，ANSYS 列: `UY`，单位: `mm`
  * `SEQV` -> GRPD: `VonMisesStress` [标量]，ANSYS 列: `SEQV`，单位: `MPa`
* **Thermal (热学场)**:
  * `TEMP` -> GRPD: `Temperature` [标量]，ANSYS 列: `TEMP`，单位: `K`

用户可通过参数额外传入通道，例如 `["UX", "SXX"]`，在对标中实现更自由的轴向位移与局部应力比对。

---

## 4. 被修改的组件与物理文件

### 4.1 [compare_and_export.py](file:///d:/Project_C++/GRPD/.agents/mcp/ansys-mcp-server/compare_and_export.py)
*   **修改点**：重构 `compare_grpd_and_ansys` 函数，实现三维向量投影过滤与自适应容差算法；重构数据加载与图表绘制，循环遍历指定的 `components`，动态生成多子图和 Excel 表格。

### 4.2 [server.py](file:///d:/Project_C++/GRPD/.agents/mcp/ansys-mcp-server/server.py)
*   **修改点**：调整 `generate_comparison_report` 微服务接口，追加 `components: list = None`, `tol: float = None`, `start_z: float = 0.0`, `end_z: float = 0.0` 等参数，并将计算结果（如各通道最大相对误差）存入 SQLite 数据库 `validation_history.db`。

### 4.3 [references/smoke-test-playbook.md](file:///d:/Project_C++/GRPD/.agents/skills/superCAE/skills/grpd-smoke-tester/references/smoke-test-playbook.md) 与 [SKILL.md](file:///d:/Project_C++/GRPD/.agents/skills/superCAE/skills/grpd-smoke-tester/SKILL.md)
*   **修改点**：更新 API 调用示例，阐明如何在新接口中传递自定义物理场分量和三维采样坐标，说明自适应容差参数的机制。
