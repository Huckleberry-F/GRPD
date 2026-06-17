# 隐式热传导求解支持任务进度 (task.md)

- [x] **Task 1: 扩展 MatrixFreeImplicitIntegrator 头文件定义**
  - [x] 步骤 1：在 `MatrixFreeImplicitIntegrator.h` 中声明一阶积分目标成员变量、备份历史变量 `foHistory_` 及 `isFirstOrder_`
  - [x] 步骤 2：在头文件中更新 `evaluateResidual`、`solveJFNK`、`solveLBFGS` 接口签名，引入 `currentDt`
- [x] **Task 2: 在 MatrixFreeImplicitIntegrator 中实现一阶分流与残差计算**
  - [x] 步骤 1：在 `MatrixFreeImplicitIntegrator.cpp` 的 `run` 开头实现一阶/二阶目标的动态提取与 `isFirstOrder_` 分流
  - [x] 步骤 2：重构 `flattenDisplacement` 和 `unflattenDisplacement`，支持一阶标量场与二阶向量场分流展平
  - [x] 步骤 3：重构 `evaluateResidual`，支持一阶隐式向后差分热流残差计算 $R_i = (T_i - T^n_i) / dt - \text{TempRate}_i$
- [x] **Task 3: 级联修改求解内核算法与主推进循环**
  - [x] 步骤 1：修改 `solveJFNK` 和 `solveLBFGS` 内部的 `evaluateResidual` 调用，透传 `currentDt`
  - [x] 步骤 2：修改 `run()` 函数，在牛顿外循环开始前进行 $T^n$ 历史备份，并解算出每步的 `currentDt` 传给求解器
- [x] **Task 4: 编译并利用热传导算例进行功能验证**
  - [x] 步骤 1：重新编译项目，验证无编译/链接报错
  - [x] 步骤 2：修改 `Examples/Box_Heat/PD.yaml` 算例为隐式求解，进行 JFNK/L-BFGS 验证，确保收敛与正确性
