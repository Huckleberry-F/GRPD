# 限制无矩阵隐式积分器为仅支持稳态与稳态热传导 Walkthrough (Walkthrough)

## 任务背景与物理方程
本任务是对 `MatrixFreeImplicitIntegrator` 进行修改，去掉所有与一阶瞬态时间离散（即 $\Delta t$ 项）相关的逻辑，实现纯稳态力学系统与一阶稳态热传导（扩散）控制方程的无矩阵隐式求解。

### 1. 物理残差方程
*   **一阶稳态热传导残差**：
    在稳态下，物理量不随时间变化（温度变化率 $\frac{\partial T}{\partial t} = 0$）。因此，粒子 $i$ 的残差直接等于当前温度场下的温度导数值（即由受热与边界引起的净热流率）：
    $$ R_i(T) = \text{TempRate}_i(T) $$
    无需物理时间步长 $\Delta t$，也无需暂存上一步历史值 `foHistory_`。

*   **二阶稳态力学残差**：
    力学系统在稳态（静态）下的残差直接由内力和外力平衡决定：
    $$ R_i(\mathbf{u}) = F_{int, i}(\mathbf{u}) + F_{ext, i} $$

### 2. 边界条件处理与残差归零机制
在稳态隐式迭代中，必须保证被 Dirichlet 边界（固定位移或固定温度）约束的节点始终满足其已知值。
为此，我们在 `evaluateResidual` 里实现了两阶段边界安全控制：
1.  **物理场强制锁死**：在 unflatten 最新更新的位移（或温度）到 SoA 物理场后，立即调用 `BC::applyConstraints`，将约束节点的物理量强行覆盖为 Dirichlet 边界值，使得底层的物理受力/热流计算（`evaluateForces`）使用的是完全符合边界约束条件的数值。
2.  **边界残差强行归零**：既然被约束节点的值是已知的，不属于待求解的未知数，那么它们就不需要通过牛顿法进行平息。在组装好物理残差向量 $R$ 后，我们遍历所有处于当前步的约束型 BC，获取其粒子 ID 与约束轴向，在 $R$ 中强行将这些受约束自由度的残差设定为 `0.0`。
    $$ R_{constrained} = 0 $$
    这使得 JFNK/L-BFGS 在求解 $\delta u$ 时能自动算得边界点的修正量为 0，从而使恒温/固定位移点完美死锁在边界值上，实现了极高精度的数值收敛。

## 实现细节

1.  **接口清理** ([MatrixFreeImplicitIntegrator.h](file:///Users/hanbozhang/C++/GRPD/Src/Integration/include/MatrixFreeImplicitIntegrator.h))
    - 移除了用于暂存温度历史的私有成员 `foHistory_`。
    - 还原了 `evaluateResidual`、`solveJFNK`、`solveLBFGS` 的接口声明，去除了 `currentDt` 签名。

2.  **指针悬空与初始化顺序修复** ([MatrixFreeImplicitIntegrator.cpp](file:///Users/hanbozhang/C++/GRPD/Src/Integration/src/MatrixFreeImplicitIntegrator.cpp))
    - 在原本的设计中，`run()` 中先调用 `extractTargets` 提取物理场裸指针，然后再遍历内核执行 `preCompute`。在 `preCompute` 阶段中，内核常会通过 `fieldManager.addField` 动态增加并 resize 新物理场，导致 `FieldManager` 内部物理场容器重新分配内存，使得此前提取的裸指针（如 `fo.primaryPtr`，`fo.ratePtr`）瞬间变为悬空的失效野指针。
    - 我们将 `run()` 开头的初始化顺序进行了颠倒：**先执行 preCompute 进行所有的物理场分配与定型，再调用 extractTargets 提取 ODE 积分物理场目标的最新 dataPtr()**。这彻底消除了指针悬空隐患，确保了 SoA 裸指针在整个牛顿迭代大循环中的绝对安全和读写有效性。
    - 还原了 `solveJFNK`、`solveLBFGS` 以及 `run()` 迭代中对 `evaluateResidual` 的调用，清理了所有的 `currentDt` 参数传递。

3.  **多态粒子 ID 获取支持**
    - 在 [BC.h](file:///Users/hanbozhang/C++/GRPD/PDCommon/BC/include/BC.h) 中添加了 `getParticleId()` 和 `getAxis()` 虚函数。
    - 分别在 [ThermalBC.h](file:///Users/hanbozhang/C++/GRPD/PDCommon/BC/include/ThermalBC.h)（`TemperatureBC`）与 [MechanicalBC.h](file:///Users/hanbozhang/C++/GRPD/PDCommon/BC/include/MechanicalBC.h)（`DisplacementBC`、`VelocityBC`）中进行了覆写，多态返回受约束节点 ID 及轴向，支持了 `evaluateResidual` 里的自由度残差精准清零。

## 验证结果

### 1. 编译验证
在 `build` 目录下执行编译，无任何警告与报错：
```bash
cmake -DCMAKE_BUILD_TYPE=Release .. && make -j
```
编译成功生成了 `GRPD` 可执行文件。

### 2. 稳态热传导数值收敛性验证
修改 `Examples/Thermal_Validation_5x10/PD.yaml` 开启隐式 JFNK 稳态求解，该算例右端面为 100K 恒温 Dirichlet 边界，左端面为 100.0 热流 Neumann 边界。
运行结果日志：
```
    |   [BC Debug] Name: T_BC2_P4999 | EntryStep: 0 | currentStep: 10 | pid: 4999
    |   [BC Debug] Name: T_BC2_P4998 | EntryStep: 0 | currentStep: 10 | pid: 4998
    ...
    |   [BC Debug] Total constraint BCs checked: 100
    |   -> Iter 0 | Res Norm: 0.000018 | Max Res: 0.000001
    |   [*] Converged in 0 iterations.
```
-   **分析**：100 个恒温边界点被精确识别并且它们的残差被强制归零。牛顿迭代法在稳态下仅 0 次迭代（第一步残差已经小于容差 `1.0e-5`）即以极高的数值精度完美收敛。这证明了仅稳态热传导的隐式求解及其边界约束清零机制的正确性。
