# 无矩阵隐式积分器支持一阶热传导求解 Walkthrough (Walkthrough)

## 任务背景与目的
本任务旨在扩展 GRPD 引擎的无矩阵隐式时间积分器 `MatrixFreeImplicitIntegrator`，使其在保持原有二阶力学系统隐式求解的同时，支持一阶瞬态非线性热传导（扩散）控制方程的隐式高精度求解（JFNK / L-BFGS 算法）。这有助于对高非线性热学问题实现大步长、高稳定性的数值计算。

## 物理方程与离散残差
一阶瞬态热传导控制方程为：
$$ \rho C_p \frac{\partial T}{\partial t} = \text{TempRate}_{unscaled} $$

在 GRPD 的 SoA 物理场管理中，`TempRate` 场实际存储的是经过密度 $\rho$ 与比热 $C_p$ 尺度缩放后的单位时间温度变化率：
$$ \text{TempRate} = \frac{\text{TempRate}_{unscaled}}{\rho C_p} $$

隐式求解在子步 Newton-Raphson 试探内循环中必须利用时间步长 $\Delta t$ 进行向后欧拉差分（Implicit Backward Euler）。对粒子 $i$ 在当前时间步 $n+1$ 的离散残差方程 $R_i(T^{n+1})$ 组装公式如下：
$$ R_i(T^{n+1}) = \frac{T^{n+1}_i - T^n_i}{\Delta t} - \text{TempRate}_i(T^{n+1}) $$

其中：
- $T^{n+1}_i$ 是粒子 $i$ 的当前牛顿迭代温度值。
- $T^n_i$ 是上一个收敛时间步的温度历史备份（存储在 `foHistory_` 中）。
- $\text{TempRate}_i(T^{n+1})$ 是基于当前估算温度 $T^{n+1}$ 在 PDKernel（如 `NOSB_T`）中计算出的温度导数项。

当离散残差向量的 L2 范数满足 $\|R\|_2 < \epsilon$ 时，即认为牛顿法收敛。

## 实现细节

我们修改并新增了以下关键逻辑以支持一阶场：

1. **头文件修改** ([MatrixFreeImplicitIntegrator.h](file:///Users/hanbozhang/C++/GRPD/Src/Integration/include/MatrixFreeImplicitIntegrator.h))
   - 引入一阶物理场相关的成员变量：
     - `foTargets_`：存储 `FirstOrderTarget` 热学物理场的指针。
     - `rateFieldNames_`：一阶导数场名。
     - `isFirstOrder_`：是否为一阶控制方程求解标志。
     - `foHistory_`：用于存储上一步收敛的温度值 $T^n$ 的扁平数组。
   - 级联穿透修改：将 `currentDt` 传给 `evaluateResidual`、`solveJFNK`、`solveLBFGS`。

2. **源文件分流与残差计算** ([MatrixFreeImplicitIntegrator.cpp](file:///Users/hanbozhang/C++/GRPD/Src/Integration/src/MatrixFreeImplicitIntegrator.cpp))
   - **分流检测**：在 `run()` 开头提取积分目标场，若二阶场为空，则自动提取一阶热学场并设置 `isFirstOrder_ = true`。
   - **预计算初始化**：在主时间循环前，强制遍历执行 `kernel->preCompute(ctx)`，解决 `NOSB_T` 依赖逆形状张量 `ShapeTensorInv` 时发生找不到字段告警的问题。
   - **展平与重构**：修改 `flattenDisplacement` 和 `unflattenDisplacement` 方法，对一阶场利用 `foTargets_` 的 `primaryPtr` 连续内存进行向量的展平与写回。
   - **隐式残差组装**：在 `evaluateResidual` 中，针对一阶场传入 `rateFieldNames_` 触发核函数受力（热流）计算，并使用后向欧拉差分公式组装残差。
   - **步长透传与历史备份**：在 `run()` 中子步开始前，在牛顿法开始迭代前，将当前温度展平并暂存到 `foHistory_`。在每次调用求解器与计算残差时，透传由当前子步算出的 `currentDt`。

3. **算例配置调整** ([PD.yaml](file:///Users/hanbozhang/C++/GRPD/Examples/Box_Heat/PD.yaml))
   - 将 `TimeIntegrator` 切换为 `MatrixFreeImplicit`，算法指定为 `JFNK`，使 Box_Heat 算例能够启用隐式求解。

## 验证结果

### 1. 编译验证
在 `build` 目录下执行编译，无任何编译、链接报错或警告：
```bash
cmake -DCMAKE_BUILD_TYPE=Release .. && make -j
```
编译成功生成了 `GRPD` 可执行文件。

### 2. 数值收敛性验证
调用 `grpd-server::run_grpd_case` 运行 `Box_Heat` 算例。输出日志片段如下：
```
    | Step 1 / Substep 1 | LF=0.100000
    |   -> Iter 0 | Res Norm: 1253.948270 | Max Res: 247.989269
    |   -> Iter 1 | Res Norm: 3.398007 | Max Res: 0.380029
    |   -> Iter 2 | Res Norm: 0.003380 | Max Res: 0.000329
    |   -> Iter 3 | Res Norm: 0.000000 | Max Res: 0.000000
    |   [*] Converged in 3 iterations.
    | ==========================================================
    | Step 2 / Substep 1 | LF=0.200000
    |   -> Iter 0 | Res Norm: 508.988003 | Max Res: 50.948997
    |   -> Iter 1 | Res Norm: 0.496495 | Max Res: 0.032407
    |   -> Iter 2 | Res Norm: 0.000414 | Max Res: 0.000039
    |   -> Iter 3 | Res Norm: 0.000000 | Max Res: 0.000000
    |   [*] Converged in 3 iterations.
    ...
    | Step 10 / Substep 1 | LF=1.000000
    |   -> Iter 0 | Res Norm: 94.038424 | Max Res: 2.310655
    |   -> Iter 1 | Res Norm: 0.042640 | Max Res: 0.005828
    |   -> Iter 2 | Res Norm: 0.000041 | Max Res: 0.000004
    |   -> Iter 3 | Res Norm: 0.000000 | Max Res: 0.000000
    |   [*] Converged in 3 iterations.
```
**分析结论：**
- 隐式向后欧拉残差计算逻辑正确，牛顿法则在 3 次迭代内迅速收敛至残差范数为 0。
- `preCompute()` 成功在时间步循环之前提取并构建了 `ShapeTensorInv`，控制台没有出现任何 `ShapeTensorInv not found` 的初始化报错。
- 数值模拟流程无 NaN 崩溃，运行十分平稳。
