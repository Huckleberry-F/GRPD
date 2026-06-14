# 固化 TIME 结果提取机制与 t=2.0s 水平中线对标计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 
1. 在 Python 的 APDL 发生器代码中进行固化修改：当传入 GRPD 大步数（如 substep=20000）且 result_time 为 0 时，根据 `TimeStep_dt` 自动换算为物理时间 `result_time = substep * dt`，从而强制 ANSYS 后处理使用物理时间 `TIME` 检索机制（`SET, , , , , TARGET_TIME`），彻底固化按照 TIME 提取结果的逻辑。
2. 不重新进行 GRPD 求解，基于现有的 `Result_20260613_121325` 计算结果，重新运行 $t=2.0\text{ s}$ 时刻中线 $(0,5,0) \to (5,5,0)$ 的 GRPD 与 ANSYS 对标验证，并进行实验数据库归档。

**Architecture:** 
1. 在 `generator.py` 中，增加 `result_time` 根据 `result_substep` 和 `TimeStep_dt` 的自动换算和固化逻辑。
2. 调用 `grpd-server.get_grpd_vtk_result` 直接在已有目录中定位第 `20000` 步对应的 VTK 文件（物理时间为 $t=2.0\text{ s}$）。
3. 调用 `ansys-server` 运行瞬态分析并使用时间检索机制提取 $t=2.0\text{ s}$ 的结果。
4. 调用 `grpd-validation-server` 进行中线对比并计算温度相对误差。
5. 完成实验数据库入库并落盘 Walkthrough 总结文件。

**Tech Stack:** Python MCP Services, APDL MAPDL, SQLite

---

## 实施步骤

### Task 1: 固化 TIME 检索代码逻辑

**Files:**
- Modify: `d:/C++pro/GRPD/.agents/mcp/ansys-mcp-server/src/generator.py:68-72`

- [ ] **Step 1: 修改 generator.py 以实现大步数向物理时间的自动换算**
  修改 `generator.py`，使之能够自动利用 `TimeStep_dt` 将离散步数 `result_substep` 转换为 `result_time`，从而在后处理中强制生成基于时间检索的命令。

```python
    if result_time <= 0.0 and result_substep > 0:
        dt = float(solver.get("TimeStep_dt", 0.0))
        if dt > 0.0:
            result_time = result_substep * dt

    if result_time > 0.0:
        output_stem = f"ansys_val_results_t{result_time}"
    else:
        output_stem = f"ansys_val_results_sub{result_substep}" if result_substep else "ansys_val_results"
```

---

### Task 2: 对标 t=2.0s 时的中线温度场

- [ ] **Step 1: 定位 GRPD t=2.0s 的 VTK 结果**
  调用 `grpd-server.get_grpd_vtk_result`。由于不进行重新求解且文件已存在，传入 `case_dir = "d:/C++pro/GRPD/Examples/Thermal_Validation_5x10"`，以及 `substep = 20000`（对应第 20000 步输出的 VTK，代表 $t=2.0\text{ s}$）。
  
- [ ] **Step 2: 调度 ANSYS 提取 t=2.0s 的水平中线结果**
  调用 `ansys-server.run_ansys_yaml_case`，指定参数：
  * `time = 2.0`
  * `start_x = 0.0`, `end_x = 5.0`
  * `start_y = 5.0`, `end_y = 5.0`
  
- [ ] **Step 3: 运行插值对比与误差分析**
  调用 `grpd-validation-server.compare_grpd_vtk_with_ansys_txt`：
  * `physics_type = "Thermal"`
  * `components = ["TEMP"]`
  * 起止坐标为 $(0,5,0) \to (5,5,0)$。
  
- [ ] **Step 4: 实验数据库归档**
  通过 `grpd-experiment-server` 进行数据入库归档，关联 `run_id` 的 $t=2.0\text{ s}$ 的误差指标与产出文件。

- [ ] **Step 5: 物理落盘 Walkthrough 并反馈**
  创建 `d:/C++pro/GRPD/docs/superpowers/plans/2026-06-13-solidify-time-extraction-and-t2-validation-walkthrough.md` 总结文件。
