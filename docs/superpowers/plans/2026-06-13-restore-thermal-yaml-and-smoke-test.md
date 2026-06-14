# 恢复历史热传导算例设置并执行自动化冒烟对标测试

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 `Thermal_Validation_5x10` 算例的 YAML 配置文件恢复为以前与 ANSYS 对标良好的两段式加载和钢材物性参数版本，然后调用三段式冒烟测试管线执行 GRPD 求解、ANSYS 求解及数值对标，最后归档至实验数据库。

**Architecture:** 
1. 覆盖修改算例的 `PD.yaml`，恢复两段式 KBC 载荷步和物理参数。
2. 使用 `grpd-server` 重新构建并运行 GRPD，提取 $t=4.0\text{ s}$ 时刻对应的第 `40000` 步 VTK 结果。
3. 调用 `ansys-server` 运行双载荷步瞬态 APDL 并自动提取 $t=4.0\text{ s}$ 的结果。
4. 调用 `grpd-validation-server` 进行一维路径插值比对，获取最大温度相对误差。
5. 将结果归档至 `grpd-experiment-server`，并输出 Walkthrough 总结文件。

**Tech Stack:** C++, GRPD Solver, ANSYS MAPDL APDL, Python MCP Services

---

## 实施步骤

### Task 1: 恢复历史 PD.yaml 配置

**Files:**
- Modify: `d:/C++pro/GRPD/Examples/Thermal_Validation_5x10/PD.yaml`

- [ ] **Step 1: 替换 PD.yaml 配置文件**
  更新 `d:/C++pro/GRPD/Examples/Thermal_Validation_5x10/PD.yaml`，包含以前的钢材参数、双载荷步和显式求解参数，同时保留 `System` 段。

```yaml
#==============================================================================
# GRPD 以前设置的瞬态热传导 5x10 对标算例 (恢复版)
#==============================================================================
System:
  Dimension: 2
  SavePath: "output"

Assembly: {}

Parts:
  - PartID: 1
    MatID: 1
    Dimension: 2
    Shape: "Box"
    Length: 5.0
    Width: 10.0
    dx: 0.1
    Thickness: 1.0
    Offset: [0.0, 0.0, 0.0]

BoundaryConditions:
  - BcID: 1
    Name: "Left_HeatFlux_100"
    PartID: 1
    Box: [-0.01, 0.10, -999.0, 999.0, -999.0, 999.0]
    Type: "FLUX"
    Value: 100.0
    Step: 0

  - BcID: 2
    Name: "Right_Fixed_Temp_100"
    PartID: 1
    Box: [4.9, 5.01, -999.0, 999.0, -999.0, 999.0]
    Type: "T"
    Value: 100.0
    Step: 0

Materials:
  - MatID: 1
    Name: "Steel"
    Type: "FourierThermalMat"
    Density: 7.85e-9
    Conductivity: 45.0
    HeatCapacity: 4.6e8

Solver:
  Engine: "PD"
  Type: "Thermal"
  TimeIntegrator: "Explicit"
  Kernel: "NOSB_T"
  LoadSteps:
    - Step: 1
      Time: 1.0
      KBC: 0
      NumSubsteps: 10
    - Step: 2
      Time: 10.0
      KBC: 1
      NumSubsteps: 90
  TimeStep_dt: 0.0001
  OutputInterval: 10000
  OMP_Threads: 16
  Horizon: 0.3015
  KernelWeightType: "Linear"
  ZeroEnergyG0: 0.1
  ZeroEnergyMethod: "Zhang"

Writer:
  Type: "binary"
  Variables:
    - Name: "PartID"
      Dimension: 1
    - Name: "Volume"
      Dimension: 1
    - Name: "Temperature"
      Dimension: 1
    - Name: "ID"
      Dimension: 1
    - Name: "HeatFlux"
      Dimension: 1
```

---

### Task 2: 执行 GRPD 求解与 ANSYS 对标

- [ ] **Step 1: 运行 GRPD 并提取 t=4.0s 的结果 VTK**
  调用 `grpd-server.get_grpd_vtk_result` 自动编译并运行。由于 $dt=0.0001\text{ s}$ 且第一步时间为 $1.0\text{ s}$，第二步进行到 $4.0\text{ s}$ 还需要 $3.0\text{ s}$。总计时间 $4.0\text{ s}$ 对应 $40000$ 步。所以 `substep` 传入 `40000`。
  
- [ ] **Step 2: 运行 ANSYS 并在 t=4.0s 时刻提取基准解**
  调用 `ansys-server.run_ansys_yaml_case`，指定 `time = 4.0`（自动触发 `SET, , , , , TARGET_TIME` 提取时间点），同时采样坐标范围与以前一致（`start_x=0.0`, `end_x=4.0` 等）。
  
- [ ] **Step 3: 调用验证模块进行插值数据误差对比**
  调用 `grpd-validation-server.compare_grpd_vtk_with_ansys_txt` 对标 `Temperature`，路径为 $(0,0,0) \to (4,8,0)$，物理类型设为 `Thermal`，待对标分量 `components=["TEMP"]`。
  
- [ ] **Step 4: 实验数据库归档**
  通过 `grpd-experiment-server` 进行数据入库：
  1. 调用 `create_experiment_run` 写入输入几何、材料与载荷参数。
  2. 调用 `finish_experiment_run` 写入 GRPD 耗时与步数。
  3. 调用 `add_experiment_metric` 录入最大温度相对误差等指标。
  4. 调用 `add_experiment_artifact` 关联对比图、Excel 报告等附件。

- [ ] **Step 5: 物理落盘 Walkthrough 并反馈**
  创建 `d:/C++pro/GRPD/docs/superpowers/plans/2026-06-13-restore-thermal-yaml-and-smoke-test-walkthrough.md` 文件，详细描述本次对标结果，并将结论总结呈现给用户。
