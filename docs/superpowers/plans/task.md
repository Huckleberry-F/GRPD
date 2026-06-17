# 多载荷步稳态隐式热传导验证任务进度 (task.md)

- [x] **Task 1: 新建 Examples 目录及配置 PD.yaml (KBC=0)**
  - [x] 步骤 1：创建文件夹 `Examples/Thermal_Validation_Steady_Steps`
  - [x] 步骤 2：创建并写入 `PD.yaml` 配置文件，包含 2 个载荷步、每步 5 子步、`KBC: 0` 和 `LoadStep_Conditions` 段
- [x] **Task 2: 执行 Python 脚本前处理**
  - [x] 步骤 1：运行前处理脚本，根据 `PD.yaml` 生成对应的几何与边界 `.grpd` 二进制模型
- [x] **Task 3: 运行 GRPD 最新二进制进行稳态隐式求解**
  - [x] 步骤 1：执行 `GRPD` 对该算例求解，收集并观察 Newton 迭代误差的收敛轨迹
- [x] **Task 4: Walkthrough 总结物理落盘**
  - [x] 步骤 1: 分析求解器收敛数据并物理创建 `2026-06-17-add-steady-loadsteps-example-walkthrough.md` 报告
- [x] **Task 5: 修复 MatrixFreeImplicitIntegrator 的局部载荷因子计算**
  - [x] 步骤 1：在 `MatrixFreeImplicitIntegrator.cpp` 中将 `activeLF` 修改为局部载荷因子，并重新编译
- [x] **Task 6: 重新验证多步稳态热传导收敛性并落盘 Walkthrough**
  - [x] 步骤 1：运行 `Thermal_Validation_Steady_Steps` 算例并捕获新的残差收敛历史
  - [x] 步骤 2：更新 Walkthrough 物理文档，并对比修复前后的收敛行为差异
- [x] **Task 7: 修复输出文件命名中的时间戳计算与子步序号传递**
  - [x] 步骤 1：在 `run()` 中引入 `globalSubstepCounter` 并在 `outputCallback` 中传递正确的物理时间 `currentTime`
- [x] **Task 8: 修复隐式迭代后 Dirichlet 边界值被覆盖的 Bug**
  - [x] 步骤 1：在牛顿迭代开始前和退出后，增加 `BC::applyConstraints` 调用进行边界“保底锁死”

