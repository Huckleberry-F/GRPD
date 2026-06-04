# GRPD 实验记录与参数影响分析

## 目标

为 GRPD 参数扫描和算例验证建立可查询历史，记录每次运行的输入参数、收敛状态、误差指标和结果路径，便于回答：

- 哪个参数组合更容易收敛？
- 哪个参数对误差影响最大？
- 最近一次代码改动是否导致算例退化？
- 某次运行的 VTK、日志、对比图、Excel 报告在哪里？

## 推荐字段

运行主记录：

- `experiment_name`：实验批次，例如 `axisymmetric-ring-horizon-sweep`。
- `case_name` / `case_dir`：算例名和路径。
- `git_commit`：代码版本。
- `solver_type`、`kernel_type`、`material_type`：核心模型信息。
- `parameters`：JSON 参数字典。
- `status`：`created`、`running`、`completed`、`failed`、`error`。
- `converged`、`num_steps`、`final_residual`、`elapsed_seconds`。
- `max_error_uy_percent`、`max_error_seqv_percent`。

常见 metric：

- `residual`
- `energy`
- `max_damage`
- `max_displacement`
- `max_von_mises`
- `max_error_uy_percent`
- `max_error_seqv_percent`

常见 artifact：

- `log`：`GRPD.log`
- `vtk`：目标 VTK
- `plot`：对比图
- `excel`：对比表
- `zip`：打包结果
- `yaml`：输入 `PD.yaml`

## MCP 工具

- `create_experiment_run`：创建运行记录。
- `finish_experiment_run`：结束运行并写入收敛/误差摘要。
- `update_experiment_run`：更新记录字段。
- `add_experiment_metric`：追加指标。
- `add_experiment_artifact`：追加输出路径。
- `import_grpd_log`：解析 `GRPD.log` 并写入收敛摘要和 residual 序列。
- `query_experiment_runs`：查询历史运行。
- `get_experiment_run`：读取完整记录。
- `compare_experiment_parameter_sweep`：比较参数扫描。
- `summarize_experiment_convergence`：汇总收敛率和 residual 范围。

## 与其他 skill 的协作

- 用 `numerical-test-generator` 设计要扫哪些参数和判据。
- 用 `grpd-smoke-tester` 或 `grpd-server` 执行算例和 ANSYS 对标。
- 用本 skill 将运行参数、收敛状态、误差和结果路径写入数据库。
