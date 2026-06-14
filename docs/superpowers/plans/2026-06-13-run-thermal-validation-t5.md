# 运行 t=5.0s 中线温度场对标验证计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 使用 $t=5.0\text{ s}$ 物理时刻和中线 $(0,5,0) \to (5,5,0)$ 采样路径重新运行 GRPD 与 ANSYS 热传导对标。修正之前的子步数 (SUBSTEP=4) 错配问题，恢复根据物理时间 (TIME=5.0) 检索结果的经典对标曲线。

**Architecture:** 
1. 提取 GRPD 求解结果中第 $50000$ 步（对应 $dt=0.0001\text{ s}$ 下的 $t=5.0\text{ s}$）的 VTK 文件。
2. 调度 `ansys-server` 运行瞬态 APDL 并以物理时间参数 `time = 5.0` 进行结果检索。
3. 对齐 $(0,5,0) \to (5,5,0)$ 路径进行一维插值误差计算。
4. 将本次对标成果归档至实验管理系统。
5. 落盘 Walkthrough 总结文件。

**Tech Stack:** GRPD Solver, ANSYS MAPDL APDL, Python MCP Services

---

## 实施步骤

### Task 1: 运行 t=5.0s 中线对标

- [ ] **Step 1: 定位 GRPD 的 t=5.0s 物理场结果**
  调用 `grpd-server.get_grpd_vtk_result`。由于已存在之前的计算结果且我们不希望重新计算（节省时间），可以直接在最新的结果目录中定位第 `50000` 步对应的 VTK。所以 `substep` 传入 `50000`。
  
- [ ] **Step 2: 调度 ANSYS 提取 t=5.0s 的中线结果**
  调用 `ansys-server.run_ansys_yaml_case`，指定参数：
  * `time = 5.0` （启用基于时间的后处理检索）
  * `start_x = 0.0`, `end_x = 5.0`
  * `start_y = 5.0`, `end_y = 5.0`
  * `substep = 0` (确保使用 TIME 而非 SUBSTEP)
  
- [ ] **Step 3: 运行插值对比与误差分析**
  调用 `grpd-validation-server.compare_grpd_vtk_with_ansys_txt`：
  * `physics_type = "Thermal"`
  * `components = ["TEMP"]`
  * 起止坐标为 $(0,5,0) \to (5,5,0)$。
  
- [ ] **Step 4: 录入实验管理数据库**
  通过 `grpd-experiment-server` 进行数据归档，关联 `run_id` 的指标和文件，标记物理参数 `target_time = 5.0`。

- [ ] **Step 5: 物理落盘 Walkthrough 并反馈**
  创建 `d:/C++pro/GRPD/docs/superpowers/plans/2026-06-13-run-thermal-validation-t5-walkthrough.md` 总结文件。
