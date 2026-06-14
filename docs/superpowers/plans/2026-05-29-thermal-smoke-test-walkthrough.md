# GRPD 热传导仿真自动化对标总结报告 (Thermal Validation Walkthrough)

本报告总结了对项目中 `Thermal_Validation_MCP` 算例执行的 GRPD 态基近场动力学（NOSB_T 积分核 + FourierThermalMat 本构）与 ANSYS 有限元（PLANE55 单元）一维稳态热传导仿真对标测试成果。

---

## 一、 仿真对象与边界条件

*   **几何尺寸**：长度 $L = 10.0\text{ mm}$，宽度 $W = 10.0\text{ mm}$，厚度 $t = 1.0\text{ mm}$。网格尺寸 $dx = 0.2\text{ mm}$，近场动力学截断半径（Horizon） $\delta = 0.603\text{ mm}$。
*   **物理材料**：导热系数 $k = 45.0\text{ W/(m}\cdot\text{K)}$，密度 $\rho = 7.85 \times 10^{-9}\text{ t/mm}^3$，比热容 $c = 4.6 \times 10^8\text{ mJ/(t}\cdot\text{K)}$。
*   **边界条件**：
    *   底端固定温度界（$y \in [-0.01, 0.21]$）：$T = 100.0\text{ K}$
    *   顶端固定温度界（$y \in [9.79, 10.01]$）：$T = 0.0\text{ K}$
    *   其余侧边绝热，实现一维垂直方向的热传导。

---

## 二、 调试与修正记录

在测试调试过程中，共发现并修正了两个核心偏差，成功将对标打通：

### 1. 离散点云采样坐标修正
*   **原配置**：采样剖面设为中线 $x = 5.0$。
*   **偏差原因**：由于 $dx = 0.2$ 且 GRPD 粒子在 x 轴呈半格点对称分布（即 $x = 0.1, 0.3, \dots, 4.9, 5.1$），$x = 5.0$ 剖面处完全没有粒子。在 pyvista 提取数据时，导致满足容差 $|x - 5.0| < 0.03$ 的点集为空，引发崩溃。
*   **修正方案**：将对标采样中线修正为实际的粒子列坐标 $x = 5.1$，此时能完美提取一整列粒子的温度场。

### 2. ANSYS 伪时间载荷步子步偏差修正
*   **原配置**：传入子步 `substep=1`，导致 APDL 生成宏执行 `SET, 1, 1`。
*   **偏差原因**：ANSYS 热力学静态分析（`ANTYPE, 0`）将时间作为伪载荷施加。由于载荷随子步线性上升（Ramped Loading），第 1 子步仅加载了总载荷的 $5\%$，即底端温度实际仅加载至 $5.0\text{ K}$。而 GRPD 侧已运行 50s 并完全收敛至 $100.0\text{ K}$ 稳态，造成了巨大的虚假温差。
*   **修正方案**：将 ANSYS 提取子步改传 `substep=0`。APDL 生成器在接收到 `0` 时，会自动生成 `SET, LAST`，以此提取 ANSYS 计算完全收敛后的 $100\%$ 满载荷状态。

---

## 三、 对标分析结果

*   **对标路径**：从 $(5.1, 0.0, 0.0)$ 到 $(5.1, 10.0, 0.0)$ 的直线。
*   **最终温度场最大相对误差**：**$1.053884\%$**
*   **误差物理解析**：
    在一维稳态热传导中，连续介质热传导方程的解析解表现为沿 $y$ 方向的线性温度分布：
    $$T(y) = 100.0 - 10.0 \cdot y$$
    非局部近场动力学（Peridynamics）的热通量通过覆盖域 $\mathcal{H}_x$ 的积分定义：
    $$\mathbf{q}(x) = \int_{\mathcal{H}_x} f(T(x'), T(x), x', x) dV'$$
    在靠近物理边界 $y = 0$ and $y = 10$ 时，由于积分半径 $\delta$ 的积分区域被物理边界截断，会产生**非局部边界效应（Non-local Surface Effect）**，使得温度梯度在边界层发生极其微小的扰动。这构成了这 $1.05\%$ 相对误差的物理来源，而非代码缺陷。
*   **结论**：误差极低且全曲线高度契合，物理对标判定为 **`[PASS]`**。

---

## 四、 成果文件归档

对标产生的核心物理文件已保存在本地递增的运行目录中：
*   **对比结果归档目录**：[run_0032](file:///d:/Project_C++/GRPD/.agents/mcp/ansys-mcp-server/work_dir/run_0032/)
*   **高保真对比曲线图**：[Comparison_Plot.png](file:///d:/Project_C++/GRPD/.agents/mcp/ansys-mcp-server/work_dir/run_0032/Comparison_Plot.png)
*   **数据对比 Excel 报表**：[Comparison_Report.xlsx](file:///d:/Project_C++/GRPD/.agents/mcp/ansys-mcp-server/work_dir/run_0032/Comparison_Report.xlsx)
*   **压缩打包数据包**：[Comparison_Output.zip](file:///d:/Project_C++/GRPD/.agents/mcp/ansys-mcp-server/work_dir/run_0032/Comparison_Output.zip)
*   **JSON 数据总结文件**：[Comparison_Summary.json](file:///d:/Project_C++/GRPD/.agents/mcp/ansys-mcp-server/work_dir/run_0032/Comparison_Summary.json)
