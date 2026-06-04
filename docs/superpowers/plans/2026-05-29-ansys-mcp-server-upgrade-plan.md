# ANSYS MCP Server Upgrade Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 升级 `ansys-mcp-server` 工具链，使其支持原生命令行执行，并新增对热传导（Thermal）算例的物理场数据提取与比较支持。

**Architecture:** 
1. `ansys_runner.py` 将增加 `argparse` 入口，使其具备 CLI 能力，并自动从 `config.yaml` 提取本地 ANSYS 安装路径。
2. `compare_and_export.py` 将引入 `physics_type` 参数，对于 `Thermal` 类型，将提取 `TEMPVAL` (ANSYS) 和 `Temperature` (GRPD) 标量场进行空间插值，并生成温度曲线对比图。

**Tech Stack:** Python 3, argparse, pyvista, pandas, matplotlib, scipy

---

### Task 1: 升级 ansys_runner.py 命令行入口

**Files:**
- Modify: `tools/ansys-mcp-server/ansys_runner.py`

- [x] **Step 1: 编写 CLI 解析与入口代码**

```python
if __name__ == "__main__":
    import argparse
    import yaml
    
    parser = argparse.ArgumentParser(description="ANSYS MAPDL Batch Runner")
    parser.add_argument("mac_file", help="Path to the APDL MAC file")
    parser.add_argument("--work_dir", default="", help="Working directory (default: same as mac file)")
    parser.add_argument("--job_name", default="", help="Job name")
    
    args = parser.parse_args()
```

- [x] **Step 2: 注入执行逻辑与读取 config.yaml**

```python
    # Try to load config.yaml for executable path
    config_path = os.path.join(os.path.dirname(__file__), "config.yaml")
    exe_path = "ansys" # fallback
    if os.path.exists(config_path):
        with open(config_path, "r", encoding="utf-8") as f:
            cfg = yaml.safe_load(f)
            exe_path = cfg.get("ansys_executable", exe_path)
            
    runner = AnsysRunner(executable=exe_path)
    res = runner.run_mac_file(
        args.mac_file, 
        work_dir=args.work_dir if args.work_dir else None, 
        job_name=args.job_name if args.job_name else None
    )
```

### Task 2: 升级 compare_and_export.py 支持 Thermal 类型

**Files:**
- Modify: `tools/ansys-mcp-server/compare_and_export.py`

- [x] **Step 1: 增加 physics_type 参数与数据读取逻辑分岔**

```python
def compare_grpd_and_ansys(
    vtk_file: str, ansys_txt_file: str, output_dir: str,
    line_start: tuple = (0.5, 0.0, 0.0), line_end: tuple = (0.5, 5.0, 0.0),
    resolution: int = 100, physics_type: str = "Mechanical"
):
    is_thermal = (physics_type.lower() == "thermal")
    
    if is_thermal:
        ansys_temp = raw_ansys[:, 1]
    else:
        ansys_uy = raw_ansys[:, 1]
        ansys_seqv = raw_ansys[:, 2]
```

- [x] **Step 2: GRPD VTK 数据对齐与空间插值**

```python
    if is_thermal:
        grpd_temp = mesh.point_data['Temperature'][mask]
        sort_idx = np.argsort(grpd_y)
        # 必须对坐标排序和去重
        
        print("3. 正在进行空间插值与误差计算...")
        f_temp = interp1d(grpd_y, grpd_temp, kind='linear', fill_value='extrapolate')
        grpd_temp_aligned = f_temp(ansys_y)
        error_temp = np.abs(grpd_temp_aligned - ansys_temp)
```

- [x] **Step 3: 图表渲染分岔与命令行入口**

```python
    if is_thermal:
        # 渲染热学单张曲线图
        fig, ax = plt.subplots(1, 1, figsize=(7, 5))
        ax.plot(grpd_y, grpd_temp, 'r-', linewidth=2, label='GRPD (Peridynamics)')
        ax.plot(ansys_y, ansys_temp, 'ko', markersize=5, label='ANSYS (FEM)')
        ax.set_xlabel('Y Coordinate (mm)')
        ax.set_ylabel('Temperature (K)')
        ax.set_title('Temperature Comparison')
    
    # 底部引入 argparse，添加 --type Thermal 支持
```

### Task 3: 清理冗余的临时调用脚本

**Files:**
- Delete: `Examples/Thermal_Validation/run_ansys.py`

- [x] **Step 1: 删除临时脚本**

```bash
Remove-Item d:\Project_C++\GRPD\Examples\Thermal_Validation\run_ansys.py
```
