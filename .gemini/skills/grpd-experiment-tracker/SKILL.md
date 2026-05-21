---
name: grpd-experiment-tracker
description: "GRPD 算例实验记录与参数影响分析入口。用于记录、查询和比较参数扫描、收敛状态、残差、耗时、误差指标、输出文件路径和运行备注；通过 grpd-experiment-server MCP 使用 SQLite 管理实验历史。"
---

# GRPD 实验记录入口

使用 `grpd-experiment-server` 记录和查询算例运行历史。这个 skill 只负责实验追踪工作流；测试方案设计仍交给 `numerical-test-generator`，完整求解/ANSYS 对标仍交给 `grpd-smoke-tester` 或 `grpd-server`。

## 必做流程

1. 读取 `references/experiment-tracking.md`，确认需要记录的参数、指标和 artifact。
2. 运行前调用 `grpd-experiment-server.create_experiment_run` 创建 run，并写入 `parameters`。
3. 运行中或运行后调用 `add_experiment_metric`、`add_experiment_artifact` 追加残差、误差、VTK、日志、图表、Excel、zip 等。
4. 有 `GRPD.log` 时优先调用 `import_grpd_log` 自动解析收敛状态、步数和最终 residual。
5. 对参数扫描结果调用 `compare_experiment_parameter_sweep`；对一批运行调用 `summarize_experiment_convergence`。

## 判断重点

- `parameters` 必须记录真实影响结果的参数，例如 horizon、dx、time step、材料参数、Kernel、Solver.Type、接触参数。
- `status` 使用稳定枚举：`created`、`running`、`completed`、`failed`、`error`。
- artifact 只保存路径，不复制大文件；路径应尽量使用绝对路径或相对仓库根目录的稳定路径。
- 参数影响分析前先确认对比的是同一算例、同一代码版本或明确记录 `git_commit`。
