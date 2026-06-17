# 无矩阵隐式积分器支持一阶热传导求解实现计划 (Implementation Plan)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 扩展 `MatrixFreeImplicitIntegrator`，使其在保留原有二阶力学无矩阵求解功能的同时，能够利用 JFNK 和 L-BFGS 算法隐式求解一阶非线性瞬态热传导问题。

**Architecture:**
1. 继承并复用基类 `TimeIntegrator::extractFirstOrderTargets`，自动识别和提取热学算子内核（如 `NOSB_T`）的 `Temperature` 与 `TempRate` 场。
2. 引入 `isFirstOrder_` 逻辑，在变量展平（flatten）、写回（unflatten）时根据物理场阶数进行分流。
3. 扩展 `evaluateResidual` 接口，传入当前子步的实际 $\Delta t$，并利用一阶向后欧拉差分公式 $R_i = \frac{T^{n+1}_i - T^n_i}{\Delta t} - \text{TempRate}_i(T^{n+1})$ 组装残差。
4. 确保在试探迭代（Inner Loop）中设置 `frozen = true` 以保护材料状态变量，收敛后统一执行 `commitState()`。

**Tech Stack:** C++17, OpenMP

---

### Task 1: 扩展 MatrixFreeImplicitIntegrator 头文件定义

**Files:**
- Modify: `Src/Integration/include/MatrixFreeImplicitIntegrator.h`

- [ ] **Step 1: 声明一阶系统相关的成员变量与修正 `evaluateResidual` 签名**

在 `MatrixFreeImplicitIntegrator` 私有区域追加以下变量：
```cpp
  // 一阶系统相关的积分目标与备份历史
  std::vector<FirstOrderTarget> foTargets_;
  std::vector<std::string> rateFieldNames_;
  bool isFirstOrder_ = false;
  std::vector<double> foHistory_; // 暂存子步开始前的历史收敛值 T^n
```
修改 `evaluateResidual`、`solveJFNK`、`solveLBFGS` 接口，引入 `currentDt` 参数：
```cpp
  std::vector<double> evaluateResidual(PDCommon::Core::PDContext &ctx, 
                                       std::vector<std::unique_ptr<PDKernel>> &kernels, 
                                       const std::vector<double>& u,
                                       int currentStep,
                                       double activeLF,
                                       double currentDt,
                                       bool frozen = false);

  std::vector<double> solveJFNK(PDCommon::Core::PDContext &ctx, 
                                std::vector<std::unique_ptr<PDKernel>> &kernels, 
                                const std::vector<double>& u_k, 
                                const std::vector<double>& R_k,
                                int currentStep,
                                double activeLF,
                                double currentDt);

  std::vector<double> solveLBFGS(PDCommon::Core::PDContext &ctx, 
                                 std::vector<std::unique_ptr<PDKernel>> &kernels, 
                                 const std::vector<double>& u_k, 
                                 const std::vector<double>& R_k,
                                 int currentStep,
                                 double activeLF,
                                 double currentDt);
```

---

### Task 2: 在 MatrixFreeImplicitIntegrator 中实现分流与一阶残差计算

**Files:**
- Modify: `Src/Integration/src/MatrixFreeImplicitIntegrator.cpp`

- [ ] **Step 1: 修改 configure 与 run 开头的目标场提取逻辑**

在 `run()` 函数开头，根据 `extractSecondOrderTargets` 的返回状态自动降级提取一阶热学场：
```cpp
  extractSecondOrderTargets(kernels, ctx, soTargets_, accFieldNames_);
  if (soTargets_.empty()) {
    extractFirstOrderTargets(kernels, ctx, foTargets_, rateFieldNames_);
    if (foTargets_.empty()) {
      LOG_ERROR("[MatrixFreeImplicit] No implicit target fields found.");
      return;
    }
    isFirstOrder_ = true;
  } else {
    isFirstOrder_ = false;
  }
```

- [ ] **Step 2: 重构 flatten/unflatten 以兼容一阶标量场**

修改 `flattenDisplacement()` 和 `unflattenDisplacement()`，在内部通过 `if (isFirstOrder_)` 实现分支：
*   对一阶场，遍历 `foTargets_` 的 `primaryPtr` 进行展平/还原。
*   对二阶场，遍历 `soTargets_` 的 `uPtr` 进行展平/还原。

- [ ] **Step 3: 重构 evaluateResidual 以计算一阶非稳态热流残差**

修改 `evaluateResidual`：
1. 一阶场调用 `evaluateForces` 时传入 `rateFieldNames_`，并利用 `ctx.setCurrentDt(currentDt)` 固化当前步长。
2. 提取计算得到的 `fo.ratePtr` (已经除以 $\rho C_p$ 的 $\text{TempRate}$)。
3. 一阶场采用后向差分残差：
   $$R[idx] = \frac{T^{n+1}_{idx} - T^n_{idx}}{\Delta t} - \text{TempRate}_{idx}$$
   其中 $T^n_{idx}$ 取自 `foHistory_`。

---

### Task 3: 级联穿透修改求解算法与主循环

**Files:**
- Modify: `Src/Integration/src/MatrixFreeImplicitIntegrator.cpp`

- [ ] **Step 1: 修改 JFNK 与 L-BFGS 内核求解器签名以传递 currentDt**

在 `solveJFNK`、`solveLBFGS` 函数的实现中，将 `currentDt` 穿透传递给所有的 `evaluateResidual` 调用。

- [ ] **Step 2: 修改 run() 函数中的主循环与历史备份**

在 `run()` 内部子步循环：
1. 计算当前子步的实际步长：
   $$currentDt = \frac{\text{config.targetTime} - \text{prevTargetTime}}{\text{config.numSubsteps}}$$
2. 如果是 `isFirstOrder_`，在 NR 牛顿外循环开始前，将当前温度展平并暂存到 `foHistory_` 中作为历史值 $T^n$。
3. 把 `currentDt` 传给所有在 `run()` 内部直接调用的 `evaluateResidual`。
4. 确认在收敛后正确执行 `executeAllRegisteredSwaps()` 和 `commitState()`。

---

### Task 4: 编译并利用热传导算例进行功能验证

**Files:**
- Test: `Examples/Box_Heat/PD.yaml`
- Test: `Examples/Thermal_Validation_5x10/PD.yaml`

- [ ] **Step 1: 重新编译项目**

在 `build/` 目录下运行：
```bash
cmake .. && make -j
```
确保无任何编译与链接错误。

- [ ] **Step 2: 利用 Box_Heat 隐式算例进行计算验证**

修改 `Examples/Box_Heat/PD.yaml`，将 `Solver.TimeIntegrator` 设置为 `MatrixFreeImplicit`，将 `Solver.Algorithm` 设置为 `"JFNK"` 或 `"L-BFGS"`。
运行求解，确认收敛过程正常，没有出现 NaN 崩溃或发散警告。
