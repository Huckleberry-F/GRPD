# 限制无矩阵隐式积分器为仅支持稳态与稳态热传导实现计划 (Implementation Plan)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 修改 `MatrixFreeImplicitIntegrator`，去掉所有一阶瞬态（瞬态热传导）所涉及的时间离散 $\Delta t$ 穿透和历史状态备份 $T^n$ 机制，仅保留稳态力学与一阶稳态热传导（扩散）的无矩阵隐式迭代求解（即求解残差 $R_i = \text{TempRate}_i = 0$ 或 $R_i = F_i = 0$）。

**Architecture:**
1. 清理 `MatrixFreeImplicitIntegrator.h` 中用于备份温度历史的成员变量 `foHistory_`。
2. 还原 `evaluateResidual`、`solveJFNK`、`solveLBFGS` 的接口签名，移除 `currentDt` 参数。
3. 修改 `evaluateResidual` 中的一阶物理残差公式：对于稳态热传导，直接组装 $R_i = \text{TempRate}_i$，无需物理时间步与历史温度，即求解稳态平衡方程 $\text{TempRate}_i = 0$。
4. 修改 `run()` 主循环：去掉在牛顿法循环前的历史温度备份 `foHistory_ = flattenDisplacement()` 逻辑，并清除所有 `currentDt` 参数传递。

**Tech Stack:** C++17, OpenMP

---

### Task 1: 清理 MatrixFreeImplicitIntegrator 头文件定义

**Files:**
- Modify: `Src/Integration/include/MatrixFreeImplicitIntegrator.h`

- [ ] **Step 1: 删除一阶温度历史备份变量，还原接口签名**

把 `MatrixFreeImplicitIntegrator` 头文件修改为：
1. 删除私有变量 `std::vector<double> foHistory_;`。
2. 还原 `evaluateResidual`、`solveJFNK`、`solveLBFGS` 接口签名，去掉 `double currentDt` 参数。

修改细节（目标接口）：
```cpp
  std::vector<double> evaluateResidual(PDCommon::Core::PDContext &ctx, 
                                       std::vector<std::unique_ptr<PDKernel>> &kernels, 
                                       const std::vector<double>& u,
                                       int currentStep,
                                       double activeLF,
                                       bool frozen = false);

  std::vector<double> solveJFNK(PDCommon::Core::PDContext &ctx, 
                                std::vector<std::unique_ptr<PDKernel>> &kernels, 
                                const std::vector<double>& u_k, 
                                const std::vector<double>& R_k,
                                int currentStep,
                                double activeLF);

  std::vector<double> solveLBFGS(PDCommon::Core::PDContext &ctx, 
                                 std::vector<std::unique_ptr<PDKernel>> &kernels, 
                                 const std::vector<double>& u_k, 
                                 const std::vector<double>& R_k,
                                 int currentStep,
                                 double activeLF);
```

---

### Task 2: 移除时间离散与修改稳态热传导残差计算

**Files:**
- Modify: `Src/Integration/src/MatrixFreeImplicitIntegrator.cpp`

- [ ] **Step 1: 还原 evaluateResidual 函数的残差组装逻辑**

1. 修改 `evaluateResidual` 的函数签名，去掉 `currentDt` 参数。
2. 针对一阶稳态热传导，移去原本的瞬态向后欧拉差分项：
   $$ R_i = \frac{T^{n+1}_i - T^n_i}{\Delta t} - \text{TempRate}_i \implies R_i = \text{TempRate}_i $$
3. 修改后的残差计算段落如下：
```cpp
  if (isFirstOrder_) {
    for (const auto& fo : foTargets_) totalSize += fo.totalComponents;
    R.resize(totalSize, 0.0);
    size_t offset = 0;
    for (const auto& fo : foTargets_) {
      for (size_t i = 0; i < fo.totalComponents; ++i) {
        // 稳态热流残差组装：直接等于当前估算下的温度导数率
        R[offset + i] = fo.ratePtr[i];
      }
      offset += fo.totalComponents;
    }
  }
```

- [ ] **Step 2: 还原 JFNK 与 L-BFGS 内部 evaluateResidual 的调用**

修改 `solveJFNK` 与 `solveLBFGS`，将其内部所有的 `evaluateResidual` 调用修改为去掉 `currentDt` 的形式。

---

### Task 3: 还原 run() 函数的时间步备份

**Files:**
- Modify: `Src/Integration/src/MatrixFreeImplicitIntegrator.cpp`

- [ ] **Step 1: 清理 run() 内部的 foHistory_ 备份与参数透传**

修改 `run()` 函数实现：
1. 删除 `foHistory_ = flattenDisplacement();` 的执行分支。
2. 将调用 `evaluateResidual`、`solveJFNK`、`solveLBFGS` 时传入的 `currentDt` 实参全部删除。

---

### Task 4: 编译与稳态算例验证

**Files:**
- Test: `Examples/Box_Heat/PD.yaml`

- [ ] **Step 1: 重新编译项目**

在 `build/` 目录下运行：
```bash
cmake -DCMAKE_BUILD_TYPE=Release .. && make -j
```
确保无编译报错。

- [ ] **Step 2: 验证 Box_Heat 稳态求解的正确性**

使用 MCP 工具 `run_grpd_case` 运行 `Box_Heat` 算例，确认稳态热传导能在一两步迭代内（或者牛顿法迭代中）收敛至 0，无 NaN 崩溃。
由于我们去掉了时间项，在运行稳态热传导时，它退化为了一个纯非线性代数方程求解器，检验其收敛历史。
