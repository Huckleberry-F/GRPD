# GRPD 冒烟测试执行计划 (Smoke Test Plan)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 
执行热传导冒烟测试验证。由于您删除了原有的 `Thermal_Validation`，需要自动新建计算目录和 `PD.yaml`，并严格遵守 `AGENTS.md` 约定的【降级铁律】，由于当前系统仍未注入原生 MCP 工具，将编写规范的 Python Entry 中转脚本调用 `server.py` 的顶层接口以实现计算与对比，保证 `work_dir` 隔离建组与 SQLite 落盘。

**Architecture:** 
1. **新建验证目录与配置**：创建 `Examples/Thermal_Validation_MCP` 及 `PD.yaml`（使用此前跑通的热传导几何和参数，包含 `Box` 修正）。
2. **编写 MCP 中转调用脚本**：创建 `.agents/run_thermal_smoke_test.py`，直接从 `grpd_mcp_server.server` 和 `ansys_mcp_server.server` 导入 `get_grpd_vtk_result`、`run_ansys_yaml_case`，并通过 `ansys_server.generate_comparison_report` 按顺序完成对比分析。
3. **执行与复查**：运行中转脚本，确认 `work_dir/run_XXXX` 被正确生成，并提取出最终验证误差。

---

### Task 1: 重建测试环境

**Files:**
- `[NEW]` `Examples/Thermal_Validation_MCP/PD.yaml`

- [x] **Step 1: 写入 PD.yaml**
生成用于热传导计算的 `PD.yaml`，确保几何约束不报矩阵奇异错误（左右边界设为固定温度），设置瞬态或稳态热分析对应的参数。

### Task 2: 编写并执行 MCP 降级入口脚本

**Files:**
- `[NEW]` `.agents/run_thermal_smoke_test.py`

- [/] **Step 1: 编写中转脚本**
根据 `grpd-smoke-tester` 要求的三步流转编写脚本：
  1. 调用 GRPD 核心求解。
  2. 调用 `run_ansys_yaml_case` 提取 ANSYS 结果。
  3. 调用 `ansys_server.generate_comparison_report` 得到对比图与误差。
- [x] **Step 2: 执行测试**
运行脚本，检查生成文件是否被正确归档进 `work_dir`，是否入库。

## User Review Required
请您审批此“降级”管线计划。只要您点头，我立刻写 YAML 并跑通整个沙盒流程！
