# 平面二维热传导算例对标验证实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 建立一个 $5 \times 10$ 的平面二维瞬态热传导对标算例。GRPD 使用 5000 个粒子（$dx=0.1$），ANSYS 使用 5000 个单元（$dx=0.1$）。左端施加热流 $HFLUX=100$，右端固定温度 $T=100$，计算时间为 5s。提取并对比 $t=2.0$s 时刻在采样路径 $(0,0)$ 到 $(3,7)$ 上的温度分布。

**Architecture:** 
1. 在 `Examples/Thermal_Validation_5x10` 文件夹中编写 `PD.yaml` 配置文件，包含稳定的材料参数、基于 primitive `Box` 的网格剖分、瞬态热学边界条件及求解配置。
2. 调度 `grpd-server` 编译并执行 GRPD 算例，生成 $t=2.0$s（第 2000 步）的 VTK 结果文件。
3. 调度 `ansys-server` 依据 `PD.yaml` 生成对应的 APDL 并求解瞬态，输出 $t=2.0$s 时采样路径上的温度数据文件。
4. 调度 `grpd-validation-server` 进行插值并对比两个求解器的温度误差，生成报告。
5. 将几何、求解配置、材料常数和最大相对误差录入 `grpd-experiment-server`。
6. 完成后物理落盘 `walkthrough.md` 文件。

**Tech Stack:** GRPD C++ Solver, ANSYS APDL, OpenMP, Python, MCP Native Tools, YAML.

---

### Task 1: 建立算例文件夹及 YAML 配置文件

**Files:**
- Create: `Examples/Thermal_Validation_5x10/PD.yaml`

- [ ] **Step 1: 创建 `PD.yaml` 配置文件**
  在 `Examples/Thermal_Validation_5x10` 目录下新建 `PD.yaml` 并写入包含 `Box` 网格划分（$dx=0.1$）、瞬态热学边界条件及稳定的材料物理量。为使显式 $dt=0.001$ 稳定，比热设为 $1.0\times 10^9$，导热系数设为 $1.0$，密度设为 $1.0\times 10^{-8}$，以确保 $dt_{crit} \approx 0.0167\text{ s} > dt = 0.001\text{ s}$。

  ```yaml
  #==============================================================================
  # GRPD 瞬态热传导 5x10 对标算例
  # 单位制：N-mm-tonne-s-K
  # ==============================================================================
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
      Name: "Left_Flux_100"
      PartID: 1
      Box: [-0.01, 0.06, -0.01, 10.01, -999.0, 999.0]
      Type: "FLUX"
      Value: 100.0
      Step: 0

    - BcID: 2
      Name: "Right_Fixed_Temp_100"
      PartID: 1
      Box: [4.94, 5.06, -0.01, 10.01, -999.0, 999.0]
      Type: "T"
      Value: 100.0
      Step: 0

  Materials:
    - MatID: 1
      Name: "Stable_Thermal_Mat"
      Type: "FourierThermalMat"
      Density: 1.0e-8         # tonne/mm³
      Conductivity: 1.0       # tonne·mm/(s³·K)
      HeatCapacity: 1.0e9      # mm²/(s²·K)

  Solver:
    Engine: "PD"
    Type: "Thermal"
    Kernel: "NOSB_T"
    TimeIntegrator: "Explicit"
    TotalTime: 5.0
    TimeStep_dt: 0.001
    OutputInterval: 2000     # 第 2000 步输出 t = 2.0s 时的结果
    Horizon: 0.3015          # 3.015 * dx
    KernelWeightType: "Linear"
    ZeroEnergyG0: 0.1
    ZeroEnergyMethod: "Zhang"
    OMP_Threads: 8
    LoadSteps:
      - Step: 1
        Time: 5.0
        KBC: 0
        NumSubsteps: 5000

  Writer:
    Type: "binary"
    Variables:
      - Name: "PartID"
        Dimension: 1
      - Name: "Volume"
        Dimension: 1
      - Name: "Temperature"
        Dimension: 1
      - Name: "HeatFlux"
        Dimension: 1
  ```

---

### Task 2: 执行 GRPD 并生成目标 VTK 结果

- [ ] **Step 1: 运行 grpd-server 以获取目标 substep 2000 ($t=2.0$s) 时的 VTK**
  调用 `grpd-server.get_grpd_vtk_result`：
  - `case_dir`: `"d:/C++pro/GRPD/Examples/Thermal_Validation_5x10"`
  - `substep`: `2000`
  期望输出：`Result_2000.vtk` 文件的绝对路径。

---

### Task 3: 执行 ANSYS 并提取对标结果

- [ ] **Step 1: 运行 ansys-server 求解并在路径上采样温度**
  调用 `ansys-server.run_ansys_yaml_case`：
  - `yaml_file`: `"d:/C++pro/GRPD/Examples/Thermal_Validation_5x10/PD.yaml"`
  - `work_dir`: `""` (留空自动隔离)
  - `time`: `2.0` (采样 t=2.0s 瞬态时刻)
  - `start_x`: `0.0`
  - `start_y`: `0.0`
  - `end_x`: `3.0`
  - `end_y`: `7.0`
  期望输出：包含 `ansys_val_results_t2.0.txt` 以及 `.db` 数据库文件的返回结果字典。

---

### Task 4: 进行对标插值和误差对比

- [ ] **Step 1: 调用 grpd-validation-server 完成结果对比**
  调用 `grpd-validation-server.compare_grpd_vtk_with_ansys_txt`：
  - `vtk_file`: Task 2 获取的 GRPD VTK 文件绝对路径
  - `ansys_txt_file`: Task 3 获取的 ANSYS txt 文件绝对路径
  - `start_x`: `0.0`
  - `start_y`: `0.0`
  - `start_z`: `0.0`
  - `end_x`: `3.0`
  - `end_y`: `7.0`
  - `end_z`: `0.0`
  - `physics_type`: `"Thermal"`
  - `components`: `["TEMP"]`
  期望输出：生成包含对比图、Excel 详细报告的验证结果，并报告最大温度误差百分比。

---

### Task 5: 实验数据库归档

- [ ] **Step 1: 调用 grpd-experiment-server 记录此次运行**
  1. 调用 `create_experiment_run`，`parameters` 填入几何尺寸、网格密度及材料常数。
  2. 调用 `finish_experiment_run` 并记录求解时长和步数。
  3. 调用 `add_experiment_metric` 记录 `max_error_temp_percent`。
  4. 调用 `add_experiment_artifact` 绑定生成的验证图像及 Excel 结果。

---

### Task 6: 物理落盘 Walkthrough 报告

- [ ] **Step 1: 创建 walkthrough 文件记录对标结果**
  在 `docs/superpowers/plans/2026-06-12-thermal-validation-walkthrough.md` 中物理保存详细的验证结论、误差曲线图并总结成果。
