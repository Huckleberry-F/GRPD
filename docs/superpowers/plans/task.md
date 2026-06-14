# GRPD 求解器自动映射与物理时间对标任务进度 (task.md)

- [x] **Task 1: 升级 ansys-mcp-server 的 APDL 生成器**
  - [x] 步骤 1：修改 `generator.py` 的 `generate_apdl_from_yaml`，支持 `result_time` 参数。
  - [x] 步骤 2：在 `generator.py` 中解析 `TimeIntegrator` 动态生成 `ANTYPE`（瞬态 `TRANS`/静态 `STATIC`）和 `TIMINT`。
  - [x] 步骤 3：在 `generator.py` 中遍历生成多载荷步 `SOLVE` 并输出。
  - [x] 步骤 4：在后处理中使用 `result_time` 进行 `SET` 时间提取对标。

- [x] **Task 2: 升级 ansys-mcp-server 服务与门面暴露**
  - [x] 步骤 1：修改 `service.py`，升级 `generate_ansys_apdl_from_yaml` 和 `run_ansys_yaml_case` 支持 `time` 参数。
  - [x] 步骤 2：修改 `server.py`，在 Tool 接口中暴露并透传 `time` 参数。

- [x] **Task 3: 升级 grpd-mcp-server 对 VTK 物理时间提取的支持**
  - [x] 步骤 1：修改 `grpd_runner.py` 的 `find_target_vtk`，正则解析并返回 `physical_time`。
