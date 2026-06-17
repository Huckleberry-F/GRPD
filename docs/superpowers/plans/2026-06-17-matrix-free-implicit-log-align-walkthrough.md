# MatrixFreeImplicitIntegrator 日志与输出逻辑对齐 Walkthrough

## 变更目的

为了提升稳态隐式求解器 `MatrixFreeImplicitIntegrator` 的日志可读性及求解流程规范性，本变更参考了 `ADR_Integrator.cpp` 中的优秀实践，将主循环结构、多载荷步参数读取、日志打印及 VTK 导出逻辑与其全面对齐。

---

## 变更内容

### 1. 初始状态输出
* 仿照 `ADR_Integrator`，在主求解循环开始前增加未变形/初始状态的 VTK 输出，调用：
  ```cpp
  int globalSubstepCounter = 0;
  if (outputCallback) {
    outputCallback(globalSubstepCounter, 0.0);
  }
  ```
  这有助于后处理中进行直观的初始状态对比。

### 2. 对齐载荷步参数与日志格式
* **载荷步循环重构**：将基于 `for (const auto& config : loadStepConfigs_)` 的简单循环重构为类似 `ADR_Integrator` 的 `size_t stepIdx` 索引循环，使得可以方便地获取上一载荷步的信息。
* **参数提取一致**：
  * `currentNumSubsteps` 提取自 `stepConfig.numSubsteps`。
  * `currentKbc` 优先提取 `stepConfig.kbc`，若未指定则退化为全局 `kbc_`。
* **日志信息对齐**：使用一致的横幅格式输出载荷步汇总信息：
  ```
  === Load Step X / Y | NumSubsteps: Z | KBC: W ===
  ```

### 3. 子步内 activeLF 与时间物理累积计算
* 将子步的循环下限设为 `0` 且小于 `currentNumSubsteps`（原为从 `1` 到 `numSubsteps`）。
* 在子步结束时，通过上一载荷步终点时间 `prevPhysicalTime` 和当前子步比例，精确插值计算出当前物理时间 `currentPhysicalTime`，以规范 VTK 文件的命名后缀。

---

## 物理文件变更详情

### [MODIFY] [MatrixFreeImplicitIntegrator.cpp](file:///Users/hanbozhang/C++/GRPD/Src/Integration/src/MatrixFreeImplicitIntegrator.cpp)

在 `MatrixFreeImplicitIntegrator::run` 中进行了循环、参数提取、日志打印和回调调用的修改：

```diff
-  int globalSubstepCounter = 0;
-  double prevTargetTime = 0.0;
-  
-  for (const auto& config : loadStepConfigs_) {
-    for (int sub = 1; sub <= config.numSubsteps; ++sub) {
-      double activeLF = 1.0;
-      double stepTimeSpan = config.targetTime - prevTargetTime;
-      double currentDt = stepTimeSpan / config.numSubsteps;
-      if (kbc_ == 0) { // Ramp
-        activeLF = static_cast<double>(sub) / config.numSubsteps; 
-      } else { // Step
-        activeLF = 1.0;
-      }
-      
-      LOG_INFO("==========================================================");
-      LOG_INFO("[MatrixFreeImplicit] Step " + std::to_string(config.stepId) + " / Substep " + std::to_string(sub) + " | LF=" + std::to_string(activeLF));
-
-      // 1. 在子步开始前，先执行边界施加，确保物理场初始值以及 flattenDisplacement() 提取的边界自由度为最新值
-      BC::applyConstraints(ctx.getBCManager(), activeLF, config.stepId);
...
+  // [初始状态输出] 开始迭代前，先输出一次初始未变形装配，便于后处理参考对比
+  int globalSubstepCounter = 0;
+  if (outputCallback) {
+    outputCallback(globalSubstepCounter, 0.0);
+  }
+  
+  for (size_t stepIdx = 0; stepIdx < loadStepConfigs_.size(); ++stepIdx) {
+    const auto &stepConfig = loadStepConfigs_[stepIdx];
+    int currentNumSubsteps = stepConfig.numSubsteps;
+    int currentKbc = (stepConfig.kbc >= 0) ? stepConfig.kbc : kbc_;
+
+    LOG_INFO("=== Load Step " + std::to_string(stepConfig.stepId) + " / " +
+             std::to_string(loadStepConfigs_.size()) +
+             " | NumSubsteps: " + std::to_string(currentNumSubsteps) +
+             " | KBC: " + std::to_string(currentKbc) + " ===");
+
+    for (int sub = 0; sub < currentNumSubsteps; ++sub) {
+      double activeLF = 1.0;
+      if (currentKbc == 0) { // Ramp
+        activeLF = static_cast<double>(sub + 1) / currentNumSubsteps; 
+      } else { // Step
+        activeLF = 1.0;
+      }
+      
+      LOG_INFO("==========================================================");
+      LOG_INFO("[MatrixFreeImplicit] Step " + std::to_string(stepConfig.stepId) + " / Substep " + std::to_string(sub + 1) + " | LF=" + std::to_string(activeLF));
+
+      // 1. 在子步开始前，先执行边界施加，确保物理场初始值以及 flattenDisplacement() 提取的边界自由度为最新值
+      BC::applyConstraints(ctx.getBCManager(), activeLF, stepConfig.stepId);
```

---

## 验证结论

### 编译验证
* 执行项目 Release 构建，编译顺利通过，未引入任何警告与冲突。

### 运行验证
* 在算例 `Examples/Thermal_Validation_Steady_Steps` 下使用更新后的可执行文件执行求解。输出日志中，载荷步总结横幅完美呈现：
  ```
  === Load Step 1 / 2 | NumSubsteps: 5 | KBC: 0 ===
  ...
  === Load Step 2 / 2 | NumSubsteps: 5 | KBC: 0 ===
  ```
* 结果 VTK 目录成功写入了 `t0.0000.vtk` 作为初始状态，且 10 个求解步文件（从 `t0.2000.vtk` 至 `t5.0000.vtk`）物理时间轴分布正确，边界温度锁死完全正常。
