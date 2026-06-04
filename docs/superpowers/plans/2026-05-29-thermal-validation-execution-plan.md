# Thermal Validation Execution Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 运行 ANSYS MAPDL 热传导对标宏，提取温度场结果，并使用新版工具链与 GRPD 的最终步 VTK 结果对齐并绘制误差分析图。

**Architecture:** 
- 调用 `ansys-server.run_ansys_yaml_case` 生成 APDL、运行 ANSYS 并导出 `ansys_val_results.txt`。
- 调用 `ansys-server.generate_comparison_report` 处理 `Result_step03740_t10.0000.vtk` 和 `ansys_val_results.txt`。

---

### Task 1: 运行 ANSYS 基准热计算

**Files:**
- Run: `Examples/Thermal_Validation/ansys_val.mac`

- [ ] **Step 1: 通过 ansys-server 启动验证计算**

```text
ansys-server.run_ansys_yaml_case(
    yaml_file="Examples/Thermal_Validation/PD.yaml",
    work_dir="<ANSYS_Work_Dir>",
    start_x=0.5,
    end_x=0.5,
    substep=<Substep_Num>
)
```

### Task 2: 执行结果交叉验证与对比图表生成

**Files:**
- Read: `Result_20260529_163429/Result_step03740_t10.0000.vtk`
- Read: `ansys_val_results.txt`

- [ ] **Step 1: 通过 ansys-server 生成对比报告**

```text
ansys-server.generate_comparison_report(
    vtk_file="Result_20260529_163429/Result_step03740_t10.0000.vtk",
    ansys_txt_file="ansys_val_results.txt",
    output_dir=".",
    start_x=0.5,
    start_y=0.0,
    end_x=0.5,
    end_y=5.0,
    physics_type="Thermal"
)
```
