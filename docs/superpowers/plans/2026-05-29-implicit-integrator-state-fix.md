# MatrixFreeImplicitIntegrator 状态污染与历史状态提交修复计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 修复 `MatrixFreeImplicitIntegrator` 在隐式迭代（JFNK/L-BFGS）期间污染材料内部状态变量的风险，并实现子步收敛后的历史状态变量提交（`commitState`）与载荷步结束后的边界条件固化。

**Architecture:** 
1. 给 `evaluateResidual` 增加 `frozen` 参数。在外循环评估真实残差时（Newton 迭代开始）使用 `frozen=false`；在 Krylov 子空间扰动评估（GMRES）及 L-BFGS 线搜索中，使用 `frozen=true` 且设置外循环迭代数大于0以确保本构以切线/割线模式（`stateMode=2`）工作，避免内循环试探污染。
2. 子步收敛后，调用 `executeAllRegisteredSwaps()` 与材料的 `commitState()` 固化历史状态。
3. 载荷步结束时调用 `BC::commitEndStep` 固化边界条件。

**Tech Stack:** C++17, GRPD Core Core/Integration.

---

### Task 1: 修改头文件声明

**Files:**
- Modify: `Src/Integration/include/MatrixFreeImplicitIntegrator.h`

- [ ] **Step 1: 修改 evaluateResidual 函数签名**

  将 `evaluateResidual` 的签名添加默认参数 `bool frozen = false`。
  ```cpp
  std::vector<double> evaluateResidual(PDCommon::Core::PDContext &ctx, 
                                       std::vector<std::unique_ptr<PDKernel>> &kernels, 
                                       const std::vector<double>& u,
                                       int currentStep,
                                       double activeLF,
                                       bool frozen = false);
  ```

---

### Task 2: 修改源文件实现

**Files:**
- Modify: `Src/Integration/src/MatrixFreeImplicitIntegrator.cpp`

- [ ] **Step 1: 更新 evaluateResidual 的实现**

  在 `evaluateResidual` 内部，使用 `ctx.setStateFrozen(frozen)`。若 `frozen` 为 `true`，则调用 `ctx.setOuterIter(1)` 以触发本构的切线预测状态；否则设为 `0`。
  ```cpp
  std::vector<double> MatrixFreeImplicitIntegrator::evaluateResidual(
      PDCommon::Core::PDContext &ctx, 
      std::vector<std::unique_ptr<PDKernel>> &kernels, 
      const std::vector<double>& u,
      int currentStep, double activeLF,
      bool frozen) {
    
    auto u_backup = flattenDisplacement();
    unflattenDisplacement(u);
    
    ctx.setStateFrozen(frozen);
    if (frozen) {
      ctx.setOuterIter(1); // 激活 stateMode = 2 (切线刚度/弹性试探)
    } else {
      ctx.setOuterIter(0); // 激活 stateMode = 0 (真实径向返回)
    }
    evaluateForces(ctx, kernels, accFieldNames_, currentStep, activeLF);
    
    size_t totalSize = 0;
    for (const auto& so : soTargets_) totalSize += so.totalComponents;
    std::vector<double> R(totalSize, 0.0);
    size_t offset = 0;
    for (const auto& so : soTargets_) {
      std::copy(so.aPtr, so.aPtr + so.totalComponents, R.begin() + offset);
      offset += so.totalComponents;
    }
    
    unflattenDisplacement(u_backup);
    return R;
  }
  ```

- [ ] **Step 2: 更新 solveJFNK 中的有限差分残差评估**

  在 `solveJFNK` 中，将有限差分的 perturbed 评估标记为 `frozen = true`：
  ```cpp
  std::vector<double> R_temp = evaluateResidual(ctx, kernels, u_temp, currentStep, activeLF, true);
  ```

- [ ] **Step 3: 更新 run 循环中的 L-BFGS 步和状态提交**

  - 在 `run` 内部，将 L-BFGS 计算 `R_new` 的残差评估标记为 `frozen = true`（属于线搜索试探步）。
  - 子步收敛后，插入状态提交逻辑。
  - 载荷步结束时调用 `BC::commitEndStep`。

  ```cpp
        // 如果是 L-BFGS，准备计算 y_k
        if (algorithm_ == "L-BFGS") {
          auto R_new = evaluateResidual(ctx, kernels, u_new, config.stepId, activeLF, true); // 冻结试探
  ```

  并在 Newton 循环结束后：
  ```cpp
        if (!converged) {
           LOG_WARNING("[MatrixFreeImplicit] Substep did not converge within MaxNRIters.");
        } else {
           // 子步收敛后落盘本构历史状态
           ctx.getFieldManager().executeAllRegisteredSwaps();
           for (auto &[matName, matPtr] : ctx.getMaterialManager().getMaterials()) {
             if (matPtr) {
               matPtr->commitState();
             }
           }
        }
        
        outputCallback(config.stepId, activeLF);
      }
      // 载荷步结束，固化边界条件起点
      PDCommon::BC::BC::commitEndStep(ctx.getBCManager(), config.stepId);
      prevTargetTime = config.targetTime;
  ```

---

### Task 3: 编译与冒烟测试验证

**Files:**
- Run: `cmake` build command and run existing tests.

- [ ] **Step 1: 重新编译工程**
  在终端中运行编译指令，验证修改后的头文件和源文件无编译期错误。

- [ ] **Step 2: 运行单元测试 / 冒烟测试**
  运行现有的材料本构测试或隐式积分器算例，确保没有引入回归风险。

---

## Verification Plan

### Automated Tests
- 在本地构建项目，确保通过编译。
- 使用 cmake 重新生成并执行 GRPD 现有的测试集。
