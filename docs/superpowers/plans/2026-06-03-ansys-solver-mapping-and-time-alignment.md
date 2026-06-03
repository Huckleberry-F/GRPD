# GRPD 求解器自动映射与物理时间对标实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 根据 GRPD YAML 中的求解器算法（Explicit、Implicit、ADR）自动配置并生成 ANSYS 的显式/隐式（瞬态/静力学）分析控制段，并在后处理中支持利用物理时间自动对标数据。

**Architecture:** 
1. 升级 `ansys-mcp-server` 中的 `generator.py`，解析 YAML `Solver` 属性，智能渲染 `ANTYPE`（瞬态/静力学分析类型）、`TIMINT`（时间积分开关），并支持多级载荷步 `LoadSteps` 的逐级 `SOLVE` 渲染。
2. 升级 `generator.py` 的后处理段，支持传入 `result_time`，利用 MAPDL `SET, , , , , TIME` 实现跨载荷步物理时间匹配。
3. 升级 `service.py` 和 `server.py`，在 `run_ansys_yaml_case` 及 `generate_ansys_apdl_from_yaml` 中透传 `time` 参数。
4. 升级 `grpd-mcp-server` 的 `grpd_runner.py`，从导出的 VTK 文件名中自动提取物理时间，在对标管线中优先使用物理时间进行 ANSYS 求解与对标提取。

**Tech Stack:** Python 3.12, MAPDL APDL, GRPD YAML, VTK, SQLite

---

## 拟创建与修改文件

### ansys-mcp-server
*   [MODIFY] [generator.py](file:///D:/Project_C++/GRPD/.agents/mcp/ansys-mcp-server/src/generator.py):
    - 升级 `generate_apdl_from_yaml` 支持 `result_time: float = 0.0` 参数。
    - 实现对 `PD.yaml` 中 `Solver.TimeIntegrator` 的自动解析。
    - 对于瞬态分析类型（`Explicit` 热传导 / 力学，或 `Implicit` 热传导），生成 `ANTYPE, TRANS` 并循环生成多载荷步 `SOLVE`。
    - 对于静态分析类型（`ADR` 或 `Implicit` 力学），生成 `ANTYPE, STATIC` 及单步 `SOLVE`。
    - 后处理支持物理时间提取：若指定了 `result_time > 0.0`，生成 `SET, , , , , result_time`。
*   [MODIFY] [service.py](file:///D:/Project_C++/GRPD/.agents/mcp/ansys-mcp-server/src/service.py):
    - 升级 `generate_ansys_apdl_from_yaml` 和 `run_ansys_yaml_case`，增加 `time: float = 0.0` 接口参数。
    - 更新内部对 `generate_apdl_from_yaml` 的调用，并自动命名结果文件为 `ansys_val_results_t{time}.txt`（若使用了时间参数）。
*   [MODIFY] [server.py](file:///D:/Project_C++/GRPD/.agents/mcp/ansys-mcp-server/server.py):
    - 在两个对标核心 Tool 的 FastMCP Facade 接口中暴露 `time` 参数。

### grpd-mcp-server
*   [MODIFY] [grpd_runner.py](file:///D:/Project_C++/GRPD/.agents/mcp/grpd-mcp-server/grpd_runner.py):
    - 在 `find_target_vtk` 中返回结果字典时，利用正则自动从 VTK 文件名中提取 `physical_time`（例如从 `Result_step10000_t1.0000.vtk` 提取物理时间 `1.0`）。

### docs/superpowers/plans
*   [NEW] [2026-06-03-ansys-solver-mapping-and-time-alignment.md](file:///D:/Project_C++/GRPD/docs/superpowers/plans/2026-06-03-ansys-solver-mapping-and-time-alignment.md): 本实施计划文件。

---

## 实施步骤

### Task 1: 升级 ansys-mcp-server 的 APDL 生成器

**Files:**
- Modify: `D:\Project_C++\GRPD\.agents\mcp\ansys-mcp-server\src\generator.py`

- [ ] **Step 1: 修改 generate_apdl_from_yaml 接口及多步瞬态/静态求解逻辑**
  修改 `generator.py`，支持 `result_time: float = 0.0`。读取 `Solver` 配置，生成对应的物理控制步。若为瞬态，遍历生成多载荷步 `SOLVE`。
  
  修改代码细节：
  ```python
  def generate_apdl_from_yaml(
      yaml_path: str,
      output_mac_path: str,
      *,
      job_name: str = "ansys_smoke_test",
      result_substep: int = 0,
      result_time: float = 0.0,
      default_start_x: float | None = None,
      default_end_x: float | None = None,
      default_start_y: float | None = None,
      default_end_y: float | None = None,
  ) -> dict[str, Any]:
      # ...
      # 1. 识别分析类型是否为瞬态
      # 对于 Thermal，Explicit 和 Implicit 均为瞬态
      # 对于 Mechanical，Explicit 亦为瞬态，而 ADR 和 Implicit 为静力学静态
      is_thermal = solver_type.lower() == "thermal"
      is_transient = False
      if is_thermal:
          if integrator.lower() in ["explicit", "implicit"]:
              is_transient = True
      else:
          if integrator.lower() in ["explicit"]:
              is_transient = True
              
      # ...
      # 2. 拼接 SOLU 求解控制部分
      apdl += [
          "/SOLU",
      ]
      
      if is_transient:
          apdl += [
              "ANTYPE, TRANS",      # 瞬态分析
              "TIMINT, ON",         # 开启时间积分 (包含热/力学惯性)
          ]
      else:
          apdl += [
              "ANTYPE, STATIC",     # 静力学/稳态分析
          ]
          
      apdl += [
          "NLGEOM, ON" if material.get("LargeDeformation", False) else "NLGEOM, OFF",
          "OUTRES, ALL, ALL",
      ]
      
      # 3. 施加多步 SOLVE 命令
      load_steps = solver.get("LoadSteps", [])
      if not load_steps:
          load_steps = [{"Step": 1, "Time": 1.0, "KBC": 0}]
          
      for step in load_steps:
          step_time = float(step.get("Time", 1.0))
          step_kbc = int(step.get("KBC", 0))
          step_substeps = int(step.get("NumSubsteps", num_substeps))
          
          if is_transient:
              apdl += [
                  f"TIME, {step_time}",
                  f"KBC, {step_kbc}",
                  f"NSUBST, {step_substeps}, {step_substeps}, {step_substeps}",
                  "SOLVE",
              ]
          else:
              # 静态分析直接求解单步
              apdl += [
                  f"KBC, {step_kbc}",
                  "SOLVE",
              ]
              break # 静态通常只需要单次求解
              
      apdl += [
          f"SAVE, {job_name}, db",
          "FINISH",
          "",
          "! Automatic post-processing data extraction",
          "/POST1",
          f"SUBSTEP = {int(result_substep)}",
          f"TARGET_TIME = {float(result_time)}",
          f"START_X = {fallback_start_x}",
          f"END_X = {fallback_end_x}",
          f"START_Y = {fallback_start_y}",
          f"END_Y = {fallback_end_y}",
      ]
      
      # 4. 根据是否传入 result_time，自动生成对应后处理加载指令
      if result_time > 0.0:
          apdl.append("SET, , , , , TARGET_TIME")
      else:
          apdl.append("SET, 1, SUBSTEP" if result_substep else "SET, LAST")
  ```

---

### Task 2: 升级 ansys-mcp-server 服务与门面暴露

**Files:**
- Modify: `D:\Project_C++\GRPD\.agents\mcp\ansys-mcp-server\src\service.py`
- Modify: `D:\Project_C++\GRPD\.agents\mcp\ansys-mcp-server\server.py`

- [ ] **Step 1: 升级 service.py 接口透传并格式化输出文件**
  修改 `generate_ansys_apdl_from_yaml` 和 `run_ansys_yaml_case`，增加 `time` 参数支持并渲染。
  
  修改代码细节：
  ```python
  def generate_ansys_apdl_from_yaml(
      yaml_file: str,
      output_mac: str,
      substep: int = 0,
      time: float = 0.0,  # 新增
      job_name: str = "ansys_smoke_test",
      start_x: float = 0.0,
      # ...
  ) -> dict:
      result = _generate_apdl_from_yaml(
          yaml_file,
          output_mac,
          job_name=job_name,
          result_substep=substep,
          result_time=time, # 透传
          # ...
      )
      
  def run_ansys_yaml_case(
      yaml_file: str,
      work_dir: str = "",
      substep: int = 0,
      time: float = 0.0,  # 新增
      start_x: float = 1.0,
      # ...
  ) -> dict:
      # ...
      # 在非模板分支调用中传入 time
      _generate_apdl_from_yaml(
          yaml_file,
          mac_file,
          job_name=job_name,
          result_substep=substep,
          result_time=time, # 新增
          # ...
      )
      
      # 在参数中传入
      parameters: dict[str, Any] = {"START_X": start_x, "END_X": end_x}
      # ...
      if time > 0.0:
          parameters["TIME"] = time
          
      # 对标输出文件名区分
      suffix = f"_t{time}" if time > 0.0 else (f"_sub{substep}" if substep else "")
      ansys_txt_file = os.path.join(work_dir, f"ansys_val_results{suffix}.txt")
  ```

- [ ] **Step 2: 升级 server.py 暴露 Tool 接口参数**
  修改 `server.py`，使 MCP 客户端可以传入 `time` 参数。
  
  修改代码细节：
  ```python
  @mcp.tool()
  def generate_ansys_apdl_from_yaml(
      yaml_file: str,
      output_mac: str,
      substep: int = 0,
      time: float = 0.0,  # 新增
      job_name: str = "ansys_smoke_test",
      start_x: float = 0.0,
      # ...
  ) -> dict:
      return _generate_ansys_apdl_from_yaml(
          yaml_file,
          output_mac,
          substep,
          time, # 透传
          # ...
      )
      
  @mcp.tool()
  def run_ansys_yaml_case(
      yaml_file: str,
      work_dir: str = "",
      substep: int = 0,
      time: float = 0.0,  # 新增
      # ...
  ) -> dict:
      return _run_ansys_yaml_case(
          yaml_file,
          work_dir,
          substep,
          time, # 透传
          # ...
      )
  ```

---

### Task 3: 升级 grpd-mcp-server 对 VTK 物理时间提取的支持

**Files:**
- Modify: `D:\Project_C++\GRPD\.agents\mcp\grpd-mcp-server\grpd_runner.py`

- [ ] **Step 1: 在 find_target_vtk 中使用正则自动解析物理时间**
  修改 `grpd_runner.py` 的 `find_target_vtk`，在确定输出 VTK 文件后，匹配其末尾的 `_t[\d.]+` 部分提取物理时间值。
  
  修改代码细节：
  ```python
  def find_target_vtk(case_dir: str, substep: int = 0) -> dict:
      # ... 原有查找逻辑保持不变
      target_vtk = max(vtks, key=os.path.getctime)
      
      # 物理时间正则匹配提取 (例如 Result_step10000_t1.0000.vtk -> 1.0)
      import re
      physical_time = 0.0
      match = re.search(r"_t([\d.]+)\.vtk$", os.path.basename(target_vtk))
      if match:
          try:
              physical_time = float(match.group(1))
          except ValueError:
              pass
              
      return {
          "success": True,
          "vtk_file": target_vtk,
          "result_folder": latest_folder,
          "substep": substep,
          "physical_time": physical_time,  # 新增输出
          "warning": warning,
      }
  ```

---

## 验证计划 (Verification Plan)

### 自动化验证与对标
1. **测试编译与运行**：
   在 IDE 窗口中重载以使两个 MCP 模块加载最新修改。
2. **单独获取 GRPD 结果**：
   调用 `grpd-server.get_grpd_vtk_result` (指定 `substep=10000`，即 $t=1.0$)。
   * 确认返回的字典中包含 `"physical_time": 1.0` 键值对。
3. **根据时间运行 ANSYS 瞬态求解**：
   调用 `ansys-server.run_ansys_yaml_case` (传入 `time=1.0`，`yaml_file` 指向 `Thermal_Validation_MCP/PD.yaml`)。
   * 确认输出的 `ansys_smoke_test.mac` 包含 `ANTYPE, TRANS` 求解。
   * 确认后处理段为 `SET, , , , , TARGET_TIME`，提取出的 `.txt` 文件名为 `ansys_val_results_t1.0.txt`。
4. **验证两端对标误差**：
   调用 `grpd-validation-server.compare_grpd_vtk_with_ansys_txt` 融合两端结果，验证误差应在 **1.0%** 以内。
