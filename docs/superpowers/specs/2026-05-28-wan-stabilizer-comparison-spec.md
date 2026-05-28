# Wan Stabilizer Implementation Comparison Spec

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:brainstorming to refine, evaluate, and design physical and mathematical consistency before code modifications.

**Goal:** 对比评估用户提供的 2D 手写解析版 Wan 稳定器与当前 GRPD 二三维通用预计算版的数学等价性与数值量级差异，明确其物理根源。

**Architecture:** 探究 2D 特化行列式求解与标准三维张量逆矩阵在力态、量纲归一化上的表达形式，并提供具体物理场景下的数值推导与量级计算。

**Tech Stack:** C++17, Peridynamics, Units & Dimensions

---

## 1. 物理公式差异

### 用户版（2D 解析手写版）
力状态的核心表达式（单键贡献）：
$$ dforce_i = C_1 \cdot z_1 \cdot V_j \cdot \omega $$
其中非仿射投影系数 $C_1$ 采用 $2 \times 2$ 逆张量的代数展开：
$$ C_1 = \frac{D_{00} \cdot K_{11} - D_{01} \cdot K_{10}}{\det(\mathbf{K}_{2D})} \cdot W_i $$
其中 $W_i$ 为节点的加权宏观体积（类似于 $m_i$），即 $W_i = \sum \omega_{ij} V_j$。

### GRPD 版（2D/3D 通用并行预计算版）
力状态的核心表达式：
$$ force_i = \omega \cdot V_j \cdot T_{i,x} $$
其中力态 $T_{i,x}$ 通过预计算的稳定张量 $\mathbf{A}_i$ 与加权影响函数之和归一化：
$$ T_{i,x} = \frac{g_0}{\sum \omega_{\text{raw}}} \cdot \left( A_{i,00} \cdot z_i \right) $$
在 2D 平面应变下，稳定张量由弹性拉梅常数直接表征：
$$ \mathbf{A}_i = 2\mu \mathbf{K}_i^{-1} + \lambda \text{tr}(\mathbf{K}_i^{-1})\mathbf{I} $$

---

## 2. 数值量级推导与对比

### 假定计算场景
* **材料性质**：钢，弹性模量 $E = 200 \text{ GPa}$，泊松比 $\nu = 0.3$。
  - 剪切模量 $\mu = 76.92 \text{ GPa} = 7.692 \times 10^{10} \text{ Pa}$。
  - 拉梅常数 $\lambda = 115.38 \text{ GPa} = 1.1538 \times 10^{11} \text{ Pa}$。
  - 平面应变弹性刚度 $D_{00} = \lambda + 2\mu = 2.692 \times 10^{11} \text{ Pa}$。
* **离散几何**：
  - 节点间距 $\Delta x = 10 \text{ mm}$。
  - 影响半径 $\delta = 30 \text{ mm}$，厚度 $t = 1.0 \text{ mm}$。
  - 局部节点的非仿射位移偏差 $z_{i,x} = 1 \ \mu\text{m}$。
  - 影响权重和 $\sum \omega \approx 2.0$。
  - 稳定权重系数 $G_0 = 0.01$。

### 场景 A：使用 MPa-mm 单位制（用户版常见单位）
* 长度以 $\text{mm}$ 计，应力与模量以 $\text{MPa}$ 计，力以 $\text{N}$ 计。
* 此时 $E = 2.0 \times 10^5 \text{ MPa}$，$\mu = 76920 \text{ MPa}$，$D_{00} = 269200 \text{ MPa}$。
* 粒子体积 $V_j = 10 \times 10 \times 1 = 100 \text{ mm}^3$。
* 形状张量对角项 $K_{00} \approx 5000 \text{ mm}^5$。
* 非仿射位移 $z_{i,x} = 0.001 \text{ mm}$。

#### 用户版计算力态大小：
1. 计算 $C_1$：
   $$ C_1 = \frac{D_{00}}{K_{00}} \cdot W_i = \frac{269200}{5000} \times (2.0 \times 100) \approx 10768 \text{ MPa/mm}^2 $$
2. 计算修正力态：
   $$ pforce_{\text{user}} = C_1 \cdot z_{i,x} \cdot V_j \cdot \omega \cdot G_0 = 10768 \times 0.001 \times 100 \times 0.5 \times 0.01 \approx \mathbf{5.38 \text{ N}} $$

---

### 场景 B：使用 Pa-m 单位制（GRPD 采用的标准单位制）
* 长度以 $\text{m}$ 计，应力与模量以 $\text{Pa}$ 计，力以 $\text{N}$ 计。
* 此时 $E = 2.0 \times 10^{11} \text{ Pa}$，$\mu = 7.692 \times 10^{10} \text{ Pa}$，$D_{00} = 2.692 \times 10^{11} \text{ Pa}$。
* 粒子体积 $V_j = 10^{-7} \text{ m}^3$。
* 形状张量对角项 $K_{00} \approx 5.0 \times 10^{-8} \text{ m}^5$。
* 逆张量对角项 $K^{-1}_{00} \approx 2.0 \times 10^7 \text{ m}^{-5}$。
* 非仿射位移 $z_{i,x} = 1.0 \times 10^{-6} \text{ m}$。

#### GRPD 版计算力态大小：
1. 计算稳定张量分量 $A_{00}$（平面应变下）：
   $$ A_{00} = 2\mu K^{-1}_{00} + \lambda (2 K^{-1}_{00}) = (2 \times 7.692 \times 10^{10} + 2 \times 1.1538 \times 10^{11}) \times 2.0 \times 10^7 \approx 7.692 \times 10^{18} \text{ Pa/m}^5 $$
2. 计算力态 $T_{i,x}$：
   $$ T_{i,x} = \frac{g_0}{\sum \omega} \cdot A_{00} z_{i,x} = \frac{0.01}{2.0} \times 7.692 \times 10^{18} \times 1.0 \times 10^{-6} \approx 3.846 \times 10^{10} \text{ N/m}^3 $$
3. 计算最终修正键力：
   $$ pforce_{\text{grpd}} = \omega \cdot V_j \cdot T_{i,x} = 0.5 \times 10^{-7} \times 3.846 \times 10^{10} \approx \mathbf{1923 \text{ N}} $$

---

## 3. 结论与量级映射

* **量级悬殊根源**：
  在 $\text{MPa-mm}$ 单位制下计算的稳定刚度 `Ai` 大小在 $10^1$ 数量级；而在标准单位制 $\text{Pa-m}$ 下，由于空间体积和坐标差被缩放了 $10^3 \sim 10^9$ 倍，导致形状张量逆矩阵数值放大了 $10^{15}$ 倍，稳定刚度矩阵 `Ai` 达到了 $10^{18}$ 数量级。
* **物理响应一致性**：
  数值的绝对尺寸随单位制发生迁移，但由于质量（密度）同样发生了相同的单位制转换，在时步推进时计算出的加速度 $\mathbf{a} = \mathbf{f}/m$ 及位移响应在物理上是完全相等的。
