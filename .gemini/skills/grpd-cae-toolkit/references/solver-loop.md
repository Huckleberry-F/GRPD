# 求解循环与状态提交

## 关键文件

- `Src/Engine/Solvers/PD/PDEngine.cpp`
- `Src/Integration/src/CentralDifference.cpp`
- `Src/Integration/src/ADR_Integrator.cpp`
- `Src/Integration/src/MatrixFreeImplicitIntegrator.cpp`
- `PDCommon/Kernel/include/PDKernel.h`

## CentralDifference 关键顺序

1. `kernel->preCompute(ctx)`。
2. CFL 检查并确定 `dt`。
3. 初始边界条件施加与初始力计算。
4. 每步输出回调。
5. `updateKinematicsStep1(dt)`。
6. `BC::applyConstraints(...)`。
7. `evaluateForces(...)`。
8. `updateKinematicsStep2(dt)`。
9. 再次施加 BC。
10. `kernel->postCompute(ctx)`。
11. `FieldManager::executeAllRegisteredSwaps()`。
12. 每个材料 `commitState()`。
13. load step 结束时 `BC::commitEndStep(...)`。

## 安全检查

- 不要在每步循环中新增 YAML 读取或大量动态分配。
- 不要绕过 Trial/Old 机制直接覆盖历史状态。
- 多 Kernel 写同一加速度或 rate 字段时，必须确认清零、累加和顺序。
- 输出时注意 step/time 和状态提交的相对时机。
