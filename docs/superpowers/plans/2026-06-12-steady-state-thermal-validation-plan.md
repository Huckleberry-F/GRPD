# 稳态热传导 5x10 对标算例实施计划 (更新版)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 更新 GRPD 与验证模块以在密度、导热系数和比热容都为 `1.0` 的工况下分析 $t=10$s 时的稳态温度分布。通过合理限制 ANSYS 的求解子步数（隐式求解大步长优化）显著加快 ANSYS 计算速度，并通过自适应/显式指定的 X 轴物理坐标作为绘图横轴进行对标。

**Architecture:** 
1. 在 `ansys-mcp-server` 里的 `service.py` 中限制 ANSYS 隐式求解的子步数（若 GRPD 子步数超过 100，则自动将 ANSYS 子步数限制为最多 100 步）。
2. 在 `comparison.py` 中维持支持通过 `x_axis_mode` 显式或自适应投影坐标轴绘制对比图。
3. 调整 `Examples/Thermal_Validation_5x10/PD.yaml`，将密度、导热系数和比热容均设为 `1.0`。
   为了保证显式热传导方程的数值稳定性，通过公式：
   $$\Delta t_{crit} = \frac{\rho C_p \Delta x^2}{7.5 k} \approx 0.00133\text{ s}$$
   帮用户计算出安全且高精度的时间步长为 $\Delta t = 0.0005\text{ s}$，总求解时间 $T = 10.0$s，共求解 $20000$ 步。
4. 顺序调度 `grpd-server`、`ansys-server` 以及 `grpd-validation-server` 进行求解与对标，并在对标后将数据归档至 `grpd-experiment-server` 实验数据库，最后物理落盘 Walkthrough 报告。

**Tech Stack:** GRPD C++ Solver, ANSYS APDL, OpenMP, Python, Matplotlib, MCP Native Tools, YAML.

---

### Task 1: 修改验证对比模块支持限制 ANSYS 求解步数

**Files:**
- Modify: `d:\C++pro\GRPD\.agents\mcp\ansys-mcp-server\src\service.py:130-136`

- [ ] **Step 1: 修改 `service.py` 中的子步数解析逻辑**

  限制注入 APDL 模板的子步数，最大不超过 100：
  ```python
        load_steps = solver.get("LoadSteps", [{}])
        raw_substeps = load_steps[0].get("NumSubsteps", 20) if load_steps else 20
        # 优化隐式求解步数，避免跟随 GRPD 显式细密步长
        num_substeps = min(100, raw_substeps) if raw_substeps > 100 else raw_substeps
  ```

---

### Task 2: 调整稳态热传导配置文件

**Files:**
- Modify: `d:\C++pro\GRPD\Examples\Thermal_Validation_5x10\PD.yaml`

- [ ] **Step 1: 修改材料参数与求解控制参数**

  - 密度、热传导、比热容都修改为 `1.0`。
  - 将总时间修改为 `10.0`s。
  - 将步长 `TimeStep_dt` 设为计算出的安全步长 `0.0005`s。
  - 将输出步长 `OutputInterval` 设为 `20000` 以对应 `t=10.0`s。
  - 修改 `LoadSteps` 使得时间段为 `10.0`，子步数为 `20000`。

  修改 `PD.yaml` 相应部分：
  ```yaml
  Materials:
    - MatID: 1
      Name: "Stable_Thermal_Mat"
      Type: "FourierThermalMat"
      Density: 1.0            # tonne/mm³
      Conductivity: 1.0       # tonne·mm/(s³·K)
      HeatCapacity: 1.0       # mm²/(s²·K)

  Solver:
    Engine: "PD"
    Type: "Thermal"
    Kernel: "NOSB_T"
    TimeIntegrator: "Explicit"
    TotalTime: 10.0
    TimeStep_dt: 0.0005
    OutputInterval: 20000     # 第 20000 步输出 t = 10.0s 时的结果
    Horizon: 0.3015          # 3.015 * dx
    KernelWeightType: "Linear"
    ZeroEnergyG0: 0.1
    ZeroEnergyMethod: "Zhang"
    OMP_Threads: 8
    LoadSteps:
      - Step: 1
        Time: 10.0
        KBC: 0
        NumSubsteps: 20000
  ```

---

### Task 3: 调度 GRPD/ANSYS 执行并进行对标

**Files:**
- Test: 调用 GRPD & ANSYS & 对标 MCP 进行测试

- [ ] **Step 1: 执行 GRPD 稳态仿真**
  调用 `grpd-server.get_grpd_vtk_result`：
  - `case_dir`: `"d:/C++pro/GRPD/Examples/Thermal_Validation_5x10"`
  - `substep`: `20000`
  期望输出：在 output 下生成 `Result_20000.vtk` 并返回其绝对路径。

- [ ] **Step 2: 执行 ANSYS 仿真并在路径上采样温度**
  调用 `ansys-server.run_ansys_yaml_case`：
  - `yaml_file`: `"d:/C++pro/GRPD/Examples/Thermal_Validation_5x10/PD.yaml"`
  - `work_dir`: `""`
  - `time`: `10.0` (采样 t=10.0s 的结果)
  - `start_x`: `0.0`
  - `start_y`: `0.0`
  - `end_x`: `3.0`
  - `end_y`: `7.0`
  - `template_name`: `"thermal_validation"`
  期望输出：返回 ANSYS 生成的对标文本文件 `ansys_val_results_t10.0.txt` 绝对路径。

- [ ] **Step 3: 调用 grpd-validation-server 完成对比**
  调用 `grpd-validation-server.compare_grpd_vtk_with_ansys_txt`：
  - `vtk_file`: Step 1 获取的 GRPD VTK 文件绝对路径
  - `ansys_txt_file`: Step 2 获取的 ANSYS txt 文件绝对路径
  - `start_x`: `0.0`
  - `start_y`: `0.0`
  - `start_z`: `0.0`
  - `end_x`: `3.0`
  - `end_y`: `7.0`
  - `end_z`: `0.0`
  - `physics_type`: `"Thermal"`
  - `components`: `["TEMP"]`
  - `x_axis_mode`: `"X"`
  期望输出：输出对比图像 `Comparison_Plot.png` 并报告温度对标的最大相对误差。

---

### Task 4: 实验入库和物理落盘 Walkthrough

**Files:**
- Create: `docs/superpowers/plans/2026-06-12-steady-state-thermal-validation-walkthrough.md`

- [ ] **Step 1: 实验数据入库**
  1. 调用 `grpd-experiment-server.create_experiment_run`，`parameters` 填入几何尺寸、网格密度及材料常数（密度 1.0, 导热系数 1.0, 比热容 1.0）。
  2. 调用 `grpd-experiment-server.finish_experiment_run` 记录仿真步数与求解用时。
  3. 调用 `grpd-experiment-server.add_experiment_metric` 记录 `max_error_temp_percent`。
  4. 调用 `grpd-experiment-server.add_experiment_artifact` 关联对比图表和 Excel 报告。

- [ ] **Step 2: 创建 walkthrough 文件记录成果**
  创建 `docs/superpowers/plans/2026-06-12-steady-state-thermal-validation-walkthrough.md` 物理存储验证的报告、误差图与结论。
