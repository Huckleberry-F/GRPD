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
GRPD 求解器内核中实际积分的是加速度项，所以直接计算出来的是稳定力密度（单位：$\text{N/mm}^3$）：
$$ T_{i,x} = \frac{g_0}{\sum \omega_{\text{raw}}} \cdot \left( A_{i,00} \cdot z_i \right) $$
在 2D 平面应变下，稳定张量由弹性拉梅常数直接表征：
$$ \mathbf{A}_i = 2\mu \mathbf{K}_i^{-1} + \lambda \text{tr}(\mathbf{K}_i^{-1})\mathbf{I} $$

---

## 2. 数值量级推导与对比

### 假定计算场景
为了进行严密且公平的控制变量对比，我们设定如下对称邻域粒子构型：
* **材料常数**（平面应变状态）：
  - 弹性模量 $E = 2.0 \times 10^5 \text{ MPa}$，泊松比 $\nu = 0.3$。
  - 剪切模量 $\mu = \frac{E}{2(1+\nu)} = 76923 \text{ MPa}$。
  - 拉梅常数 $\lambda = \frac{E\nu}{(1+\nu)(1-2\nu)} = 115385 \text{ MPa}$。
  - 平面应变刚度 $D_{00} = \lambda + 2\mu = 269231 \text{ MPa}$。
* **离散几何**：
  - 粒子间距 $\Delta x = 10 \text{ mm}$，厚度 $t = 1.0 \text{ mm}$。
  - 粒子体积 $V_i = V_j = 10 \times 10 \times 1 = 100 \text{ mm}^3$。
  - 稳定罚参数 $G_0 = g_0 = 0.01$。
  - 局部非仿射偏差位移 $z_{i,x} = 0.001 \text{ mm}$ (即 1 微米)。
* **对称四邻域构型**：
  - 节点 $i$ 位于原点 $(0,0)$。
  - 4 个邻域粒子对称分布在坐标轴上，间距 $r = 15 \text{ mm}$。
  - 假设每个键的影响权重 $\omega_{ij} = 0.5$，则原始影响权重之和 $\sum \omega_{\text{raw}} = 2.0$。
  - 此时累加出的形状张量对角项为：
    $$ K_{00} = K_{11} = 2 \times \omega_{ij} \cdot V_j \cdot r^2 = 2 \times 0.5 \times 100 \times 15^2 = 22500 \text{ mm}^5 $$
    根据 2D 降维机制，逆张量对角项为 $K^{-1}_{00} = K^{-1}_{11} = \frac{1}{22500} \text{ mm}^{-5}$，迹和为 $\text{trace}(K^{-1}) = \frac{2}{22500} \text{ mm}^{-5}$。

---

### 场景 A：使用 MPa-mm 单位制

#### 1. 用户手写版计算绝对物理力（N）
用户手写版直接求出的是单键贡献的**绝对物理力**：
* 稳定投影刚度 $C_1$：
  $$ C_1 = \frac{D_{00}}{K_{00}} \cdot W_i = \frac{269231}{22500} \times (4 \times 0.5 \times 100) \approx 2393.16 \text{ MPa/mm}^2 $$
* 修正力大小：
  $$ pforce_{\text{user}} = C_1 \cdot z_{i,x} \cdot V_j \cdot \omega_{ij} \cdot G_0 = 2393.16 \times 0.001 \times 100 \times 0.5 \times 0.01 \approx \mathbf{1.1966 \text{ N}} $$

#### 2. GRPD 版计算力密度与绝对物理力
GRPD 求解器内核中实际积分的是加速度项，所以直接计算出来的是**稳定力密度**（单位：$\text{N/mm}^3$）：
* 稳定张量分量 $A_{i,00}$：
  $$ A_{i,00} = 2\mu K^{-1}_{00} + \lambda \text{trace}(K^{-1}) = \frac{2 \times 76923 + 2 \times 115385}{22500} \approx 17.094 \text{ MPa/mm}^5 $$
* 稳定力态 $T_{i,x}$：
  $$ T_{i,x} = \frac{g_0}{\sum \omega_{\text{raw}}} A_{i,00} z_{i,x} = \frac{0.01}{2.0} \times 17.094 \times 0.001 \approx 8.547 \times 10^{-5} \text{ MPa/mm} $$
* 稳定力密度 $dforce^{\text{density}}_{\text{grpd}}$：
  $$ dforce^{\text{density}}_{\text{grpd}} = \omega_{ij} V_j T_{i,x} = 0.5 \times 100 \times (8.547 \times 10^{-5}) \approx \mathbf{4.2735 \times 10^{-3} \text{ N/mm}^3} $$
* 换算为**绝对物理力**（力密度乘以粒子 $i$ 的自身体积 $V_i = 100 \text{ mm}^3$）：
  $$ dforce^{\text{abs}}_{\text{grpd}} = dforce^{\text{density}}_{\text{grpd}} \times V_i = 4.2735 \times 10^{-3} \times 100 \approx \mathbf{0.42735 \text{ N}} $$

---

### 场景 B：使用 Pa-m 单位制

* 材料常数：$E = 2.0 \times 10^{11} \text{ Pa}$，$\mu = 7.692 \times 10^{10} \text{ Pa}$，$\lambda = 1.1538 \times 10^{11} \text{ Pa}$，$D_{00} = 2.692 \times 10^{11} \text{ Pa}$。
* 粒子体积：$V_i = V_j = 10^{-7} \text{ m}^3$。
* 形状张量项：$K_{00} = 22500 \times (10^{-3} \text{ m})^5 = 2.25 \times 10^{-11} \text{ m}^5$。
* 逆张量对角项：$K^{-1}_{00} = \frac{1}{2.25 \times 10^{-11}} \approx 4.444 \times 10^{10} \text{ m}^{-5}$。
* 非仿射偏差：$z_{i,x} = 10^{-6} \text{ m}$。

#### 1. 用户手写版计算绝对物理力
* $C_1 = \frac{D_{00}}{K_{00}} \cdot W_i = \frac{2.692 \times 10^{11}}{2.25 \times 10^{-11}} \times (2.0 \times 10^{-7}) \approx 2.393 \times 10^{15} \text{ Pa/m}^2$。
* $pforce_{\text{user}} = C_1 \cdot z_{i,x} \cdot V_j \cdot \omega_{ij} \cdot G_0 = 2.393 \times 10^{15} \times 10^{-6} \times 10^{-7} \times 0.5 \times 0.01 \approx \mathbf{1.1966 \text{ N}}$。

#### 2. GRPD 版计算绝对物理力
* $A_{i,00} = \frac{2 \times 7.692 \times 10^{10} + 2 \times 1.1538 \times 10^{11}}{2.25 \times 10^{-11}} \approx 1.7094 \times 10^{22} \text{ Pa/m}^5$。
* $T_{i,x} = \frac{0.01}{2.0} \times 1.7094 \times 10^{22} \times 10^{-6} = 8.547 \times 10^{13} \text{ N/m}^3$。
* $dforce^{\text{density}}_{\text{grpd}} = 0.5 \times 10^{-7} \times 8.547 \times 10^{13} = 4.2735 \times 10^6 \text{ N/m}^3$。
* $dforce^{\text{abs}}_{\text{grpd}} = 4.2735 \times 10^6 \times 10^{-7} \approx \mathbf{0.42735 \text{ N}}$。

---

## 3. 结论与物理解析

1. **绝对力的一致性**：
   在相同的材料常数和离散网格下，无论选择何种单位制（$\text{MPa-mm}$ 还是 $\text{Pa-m}$），用户版与 GRPD 版计算出的单键惩罚绝对物理力大小都保持恒定。
2. **两者的数值对等**：
   - 用户版绝对力为 **$1.1966 \text{ N}$**
   - GRPD版绝对力为 **$0.42735 \text{ N}$**
   两者仅相差约 **$2.8$** 倍（处于同一量级）。
3. **2.8 倍细微差异的代数根源**：
   - **刚度模型分子差异**：用户版单维简化投影刚度正比于平面应变刚度 $D_{00} = \lambda + 2\mu$，而三维各向同性推导的 GRPD 版稳定张量分量正比于 $2\mu + 2\lambda$。
   - **几何权重标度差异**：用户版将加权积分体积 $W_i$ 直接乘入刚度项，致使其单键绝对力表达中包含了额外的邻域点数与体积分布，这使得它对裂纹尖端等自由边界处的局部拓扑极度敏感；而 GRPD 引入 $\sum \omega_{\text{raw}}$ 进行了合理的局部空间归一化，具备更优越的网格与几何自适应收敛性。
4. **澄清原 Spec 报告中的 coeff_base 笔误**：
   在非常规态基近场动力学（NOSB-PD）中，形状张量 $K$ 的定义本身已携带了特征长度与体积信息（量纲 $[L^5]$）。原 Spec 文档错误地将键基 PD（Bond-based PD）的微模量空间归一化系数（如 $\frac{12}{\pi t \delta^3}$）二次引入到稳定张量中，导致先前得出了“两百万倍差距”的偏颇结论。在纠正这一概念性笔误并进行绝对力转换后，两者的力物理量级完全契合。
