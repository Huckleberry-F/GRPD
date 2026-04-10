# GRPD 接触模块(Contact Module)架构与实施计划

## 一、 顶层架构设计：极度解耦的外源项注入模式

### 1. 核心思想
接触模块**绝对不直接修改**粒子的位移、速度或内部本构状态（如损伤、塑性应变）。
接触模块只需接收只读的全局粒子状态（ParticleState），计算完成后，仅输出增量形式的**接触力向量（ContactForce）**和**接触热通量向量（ContactHeat）**。

### 2. 注入点设计
在 `TimeIntegrator::evaluateForces()` 计算流程中，此阶段与设定边界条件（BC Neumann 注入）处于同级地位。PDE 求解器只需在时间积分前将接触计算产生的源项叠加即可：

```text
evaluateForces() 执行流程:
  1. 清零率场 (TempRate, Acceleration)
  2. 边界条件(BC) Neumann 源项注入
  3. 接触力(Contact) 源项注入  <-- 【新增注入点】
  4. Kernel内力与本构计算
```

---

## 二、 C++ 软件工程架构设计

为了保持与现有 GRPD 架构（Manager + Registry + Factory）高度一致，我们将创建类似 Material 和 BC 模块的结构体系。

### 1. 目录结构
在 `PDCommon` 下新建 `Contact` 模块：
```text
PDCommon/
  ├── Contact/
  │   ├── include/
  │   │   ├── ContactManager.h      # 接触管理器 (PDContext成员)
  │   │   ├── ContactRegistry.h     # 接触算法单例注册器
  │   │   ├── IContactAlgorithm.h   # 接触本构算法基类
  │   │   ├── ContactSearch.h       # 搜索与接触对数据结构
  │   │   └── PenaltyContact.h      # 罚函数法具体实现
  │   └── src/
```

### 2. 核心组件说明

*   **ContactRegistry (全局单例)**: 只负责注册各种接触工厂函数（对应现有的 `MaterialRegistry`），运用宏自动注册机制（如 `REGISTER_CONTACT_TYPE`）。
*   **ContactManager (非单例)**: 作为 `PDContext` 的成员变量（这点很关键，避免全局单例污染独立求解环境）。负责管理当前上下文的接触对定义、运行时的动态接触判定以及在每步触发计算。
*   **IContactAlgorithm (算法基类)**: 提供统一的 `computeContact()` 虚函数接口，规范化所有派生接触本构的输入输出。
*   **物理场一致性与表面点获取**: 运用邻域完整度（Volume Deficit Method）估算粒子表面的法向量。接触搜索基于此机制区分出有效表面粒子群，避免内部粒子参与盲目搜索。

---

## 三、 高性能空间搜索策略

采用 **CLL (Cell-Linked List) + Verlet Skin List** 的混合加速搜索策略，完全复用目前已有的 `NeighborList` 底层构造。

1.  **静态过滤**：初始化时通过计算近场区域的缺失体积判断。
2.  **动态更新**：`ContactManager` 主动查询 `FieldManager` 中的 `damage`（损伤）场。一旦监测到断裂暴露出的新表面，便将其动态加入搜索树。
3.  **局部加速**：将搜索半径略微放大构建 Verlet 缓冲列表，降低每步搜索树重建的性能消耗。

---

## 四、 OpenMP 并行与计算策略

### 1. 罚函数法 (Penalty Method)
显式动力学下最完美的解耦做法，基于虚拟“弹簧刚度”推开穿透表面，避免繁重矩阵运算：
$$k_p = \alpha \cdot \frac{E \cdot A_{\text{segment}}}{l_{\text{min}}}$$

### 2. 并行多线程处理
*   **调度**: 对于接触对极不均衡的情况，运用 `omp parallel for schedule(dynamic)` 消除线程饥饿。
*   **数据安全**: 由于接触力需要回写到粒子（作用与反作用力双向叠加），强烈建议先期采用 `pragma omp atomic` 或者图着色（Graph Coloring）技术来解决多线程向同一个粒子写入接触力的数据竞争问题。

---

## 五、 分期实施路线图 (Roadmap)

建议分三期逐步迭代，每期完成均可单独跑算例验证。

### 🔴 第一期：基础架构搭建与表面点识别
**目标**：打通代码链路，为接触提供准确的表面对象。
*   **任务 1**：在 `PDCommon` 中新建 `Contact` 模块，完成 `ContactManager`、`ContactRegistry` 骨架类。
*   **任务 2**：在 `PDContext` 和 `EngineManager` 中挂载接触管理器，打通 `evaluateForces()` 的接触力计算虚调用。
*   **任务 3**：实现 Volume Deficit Method，遍历全场标记表面粒子。
*   **验收标准**：能够跑通基础算例，通过 VTK / Paraview 成功输出并显示出正确的“表面点”标识场（例如一个立方体的外形能被完美包裹出来）。

### 🟡 第二期：实现距离搜索与全局显式无摩擦自接触 (Global Penalty Contact)
**目标**：具备默认开启的防止物体互相贯穿的自接触能力，通过完善 YAML 参数提供可调控的防碰与初始容差接口。

#### 1. 算法重构细节：罚函数、网格换算与过盈容差
*   **接触安全距离 ($d_{safe}$)**：采用真实立方体体素换算。由于微积分网格体积 $V_i = dx_i^3$，等效边长 $dx_i = \sqrt[3]{V_i}$ （避免直接用球体公式算出偏大的有效半径）。发生相遇时，两粒子的理想脱离距离为 $d_{safe} = (dx_i + dx_j) / 2$。
*   **引入 Pinball 判断容盈**：配置 Pinball 参数用于划定“可信接触区”或提供初始越界宽容。只有当两点相距落在合法深度带时才提供法向斥力；若模型天生装配存在巨大的初始穿透，超越 Pinball 深度则会被忽略或削弱，从而保留过盈配合求解可能，防止初始瞬间接触力计算出无穷大直接炸飞模型。
*   **法向惩罚刚度 ($k_n$)**：完全暴露给 YAML 的 `Contact` 配置模块，用户自主填入具体的模量级常数，而非引擎黑盒抓取，保证工业级的可控性。

#### 2. YAML 参数接口设计
将接触模块挂载入口规范为如下结构供 `InitContact` 读取解析：
```yaml
Solver:
  Contact:
    DisableContact: false       # 给简单算例留的后门：开 true 即可彻底关闭计算，避免额外开销
    PenaltyStiffness: 1.0e10    # 罚刚度
    PinballRatio: 0.5           # 过盈区探测容差点乘子
```

*   **任务 1**：在初始化构建流 (`InitContact.cpp`) 中新增解析挂载逻辑。没被关闭时生成唯一的 `Global_Self_Contact` 压入 `ContactManager`。
*   **任务 2**：编写 `PenaltyContact` 中 O(N) 级别基于 `IsSurface=1` 的表面粒子快照构网算法 (`DynamicCellList`)，实现降维空间搜索。
*   **任务 3**：根据 $k_n \times (d_{safe} - d)$ 计算斥力反发并通过 `omp atomic` 双向叠加至各自加速场 $a_i = F / (\rho \cdot V_i)$ 中。
*   **验收标准**：依靠子弹碰撞范例观察是否出现真实反弹并利用 YAML 测试开/关和刚度的调整效能。

### 🟢 第三期：摩擦引入、热力耦合与动态断裂追踪
**目标**：满足高速侵彻、爆炸飞散下的复杂接触需求。
*   **任务 1**：引入库仑摩擦力逻辑，并利用相对滑移速度做功换算为摩擦热（Contact Heat flux）。
*   **任务 2**：在每步迭代前加入对损伤场 `damage` 的探查事件，动态暴露并激活因断裂破损产生的新生表面点。
*   **任务 3**：引入 Verlet Skin List 优化搜索效率。
*   **验收标准**：测试一个高速弹丸侵彻靶板算例，观察到弹丸与靶板接触部位产生摩擦热，以及破片断裂后的再次碰撞情况。
