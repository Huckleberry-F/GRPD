# [Active Skill: writing-plans]
# 修复 ANSYS APDL 边界载荷畸变并引入内置 APDL 模板机制实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 修复 ANSYS APDL 自动生成中的角落节点热流双向溢出问题（改用几何线/面加载），并在 MCP 服务中引入类似于 MATLAB 的内置经典 APDL 模板加载机制，以提升常规算例及经典 Benchmark 的仿真对标精度与鲁棒性。

**Architecture:** 
1. 优化 `generator.py` 中的 `_append_bc`，计算 Box 各维跨度以推断几何面法向，使用 2D 的 `LSEL`/`SFL` 和 3D 的 `ASEL`/`SFA` 代替节点级 `SF` 以隔绝多向溢热。
2. 在 `ansys-mcp-server` 中新建 `templates` 目录。
3. 在 `service.py` 中引入 `template_name` 路由参数。当指定了模板（如 `thermal_validation`）时，读取模板文件，动态解析并注入 YAML 的材料与几何参数，替代 `generator.py` 拼凑的代码，执行高精度的 Benchmark 仿真。

**Tech Stack:** Python 3.12, MAPDL APDL, YAML, SQLite

---

## 拟创建与修改文件

### ansys-mcp-server
*   [NEW] [thermal_validation.mac](file:///D:/Project_C++/GRPD/.agents/mcp/ansys-mcp-server/templates/thermal_validation.mac): 热传导一维精细对标手写模板，使用 `LSEL`/`SFL` 加载。
*   [MODIFY] [generator.py](file:///D:/Project_C++/GRPD/.agents/mcp/ansys-mcp-server/src/generator.py): 修改 `_append_bc` 实现热流面载荷的几何级生成。
*   [MODIFY] [service.py](file:///D:/Project_C++/GRPD/.agents/mcp/ansys-mcp-server/src/service.py): 在 `run_ansys_yaml_case` 中新增并实现内置模板路由逻辑。
*   [MODIFY] [server.py](file:///D:/Project_C++/GRPD/.agents/mcp/ansys-mcp-server/server.py): 在 `run_ansys_yaml_case` 门面工具中暴露 `template_name` 参数。

---

## 实施步骤

### Task 1: 建立内置模板目录与 Thermal 精准对标模板

**Files:**
- Create: `D:\Project_C++\GRPD\.agents\mcp\ansys-mcp-server\templates\thermal_validation.mac`

- [x] **Step 1: 创建并编写热传导精准对标 APDL 模板**
  创建 [thermal_validation.mac](file:///D:/Project_C++/GRPD/.agents/mcp/ansys-mcp-server/templates/thermal_validation.mac)。模板中使用参数变量占位符，支持从 Python 动态注入，并使用 `LSEL`/`SFL` 严格在几何边界线施加热流，避免角落多重受热。
  写入内容：
  ```apdl
  !==============================================================================
  ! 内置热传导精细化对标 APDL 模板 (LSEL + SFL 边界载荷)
  !==============================================================================
  FINISH
  /CLEAR, NOSTART
  /PREP7

  ! 普通2D面单元配置 (PLANE55 - 平面热学)
  ET, 1, 55
  KEYOPT, 1, 3, 0

  ! 材料属性 (由 Python 注入)
  MP, KXX, 1, MAT_K
  MP, DENS, 1, MAT_RHO
  MP, C, 1, MAT_C

  ! 建立截面几何
  RECTNG, PART_X1, PART_X2, PART_Y1, PART_Y2
  ESIZE, PART_DX
  MSHAPE, 0, 2D
  MSHKEY, 1
  AMESH, ALL

  ! 施加常驻边界条件 (左垂直线施加热流，顶部和底部绝热)
  LSEL, S, LOC, X, BC_FLUX_XMIN, BC_FLUX_XMAX
  SFL, ALL, HFLUX, BC_FLUX_VAL
  LSEL, ALL

  ! 右侧固定温度
  NSEL, S, LOC, X, BC_TEMP_XMIN, BC_TEMP_XMAX
  D, ALL, TEMP, BC_TEMP_VAL
  ALLSEL, ALL

  ! 求解配置
  /SOLU
  ANTYPE, 0
  NLGEOM, OFF
  KBC, 0
  NSUBST, SOL_SUBSTEPS, SOL_SUBSTEPS, SOL_SUBSTEPS
  OUTRES, ALL, ALL
  SOLVE
  SAVE, ansys_smoke_test, db
  FINISH

  ! 后处理采样线提取
  /POST1
  SET, LAST
  PATH, MyPath, 2, 30, 10
  PPATH, 1, 0, SAMPLE_START_X, SAMPLE_START_Y, 0.0
  PPATH, 2, 0, SAMPLE_END_X, SAMPLE_END_Y, 0.0
  PDEF, TEMPVAL, TEMP
  /OUTPUT, ansys_val_results, txt
  PRPATH, TEMPVAL
  /OUTPUT
  ```

---

### Task 2: 优化 generator.py 中的边界条件拼接逻辑

**Files:**
- Modify: `D:\Project_C++\GRPD\.agents\mcp\ansys-mcp-server\src\generator.py:219-245`

- [x] **Step 1: 修改 _append_bc 函数，实现标量热流的几何线/面施加**
  通过判断 Box 在 $X, Y, Z$ 各方向的几何范围跨度，推断其边界法向，改用 `LSEL`/`SFL` 或 `ASEL`/`SFA` 命令施加。
  修改如下：
  ```python
  def _append_bc(apdl: list[str], bc: dict[str, Any], dim: int, is_thermal: bool) -> None:
      box = bc.get("Box", [])
      bc_type = bc.get("Type", "")
      value = bc.get("Value", 0.0)

      is_flux = is_thermal and bc_type.upper() == "FLUX"

      if is_flux:
          # 根据 Box 的几何跨度推断其边界物理法向，锁定几何边界防止多维溢热
          if dim == 3 and len(box) >= 6:
              dx_box = abs(box[1] - box[0])
              dy_box = abs(box[3] - box[2])
              dz_box = abs(box[5] - box[4])
              min_dim = min(dx_box, dy_box, dz_box)
              if min_dim == dx_box:
                  apdl += [
                      f"ASEL, S, LOC, X, {box[0]}, {box[1]}",
                      f"SFA, ALL, 1, HFLUX, {value}",
                      "ASEL, ALL",
                  ]
              elif min_dim == dy_box:
                  apdl += [
                      f"ASEL, S, LOC, Y, {box[2]}, {box[3]}",
                      f"SFA, ALL, 1, HFLUX, {value}",
                      "ASEL, ALL",
                  ]
              else:
                  apdl += [
                      f"ASEL, S, LOC, Z, {box[4]}, {box[5]}",
                      f"SFA, ALL, 1, HFLUX, {value}",
                      "ASEL, ALL",
                  ]
          else:  # 2D 问题
              dx_box = abs(box[1] - box[0])
              dy_box = abs(box[3] - box[2])
              if dx_box < dy_box:
                  apdl += [
                      f"LSEL, S, LOC, X, {box[0]}, {box[1]}",
                      f"SFL, ALL, HFLUX, {value}",
                      "LSEL, ALL",
                  ]
              else:
                  apdl += [
                      f"LSEL, S, LOC, Y, {box[2]}, {box[3]}",
                      f"SFL, ALL, HFLUX, {value}",
                      "LSEL, ALL",
                  ]
      else:
          if len(box) >= 6 and dim == 3:
              apdl += [
                  f"NSEL, S, LOC, X, {box[0]}, {box[1]}",
                  f"NSEL, R, LOC, Y, {box[2]}, {box[3]}",
                  f"NSEL, R, LOC, Z, {box[4]}, {box[5]}",
              ]
          elif len(box) >= 4:
              apdl += [
                  f"NSEL, S, LOC, X, {box[0]}, {box[1]}",
                  f"NSEL, R, LOC, Y, {box[2]}, {box[3]}",
              ]

          if is_thermal:
              if bc_type.upper() in {"T", "TEMP"}:
                  apdl.append(f"D, ALL, TEMP, {value}")
          else:
              if bc_type in {"UX", "UY"} or (bc_type == "UZ" and dim == 3):
                  apdl.append(f"D, ALL, {bc_type}, {value}")
          apdl += ["ALLSEL, ALL", ""]
  ```

---

### Task 3: 优化 service.py 与 server.py 实现内置模板路由

**Files:**
- Modify: `D:\Project_C++\GRPD\.agents\mcp\ansys-mcp-server\src\service.py:83-118`
- Modify: `D:\Project_C++\GRPD\.agents\mcp\ansys-mcp-server\server.py:88-109`

- [x] **Step 1: 修改 service.py 的 run_ansys_yaml_case 增加模板加载逻辑**
  解析 `template_name`，如传入该值且对应的 `.mac` 模板文件存在，读取并根据 YAML 参数动态完成模板中占位参数的字符串替换和写入。
  修改如下：
  ```python
  def run_ansys_yaml_case(
      yaml_file: str,
      work_dir: str = "",
      substep: int = 0,
      start_x: float = 1.0,
      end_x: float = 1.0,
      start_y: float = 0.0,
      end_y: float = 0.0,
      job_name: str = "ansys_smoke_test",
      template_name: str = "",
  ) -> dict:
      """Generate APDL from GRPD YAML or templates, run ANSYS, and return results."""
      if not work_dir:
          work_dir = get_next_work_dir(default_work_dir_base())
      else:
          work_dir = os.path.abspath(work_dir)
      os.makedirs(work_dir, exist_ok=True)

      mac_file = os.path.join(work_dir, f"{job_name}.mac")
      
      # 内置模板加载路径
      templates_dir = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "templates")
      template_path = os.path.join(templates_dir, f"{template_name}.mac") if template_name else ""

      if template_name and os.path.isfile(template_path):
          print(f"[Ansys Service] Using built-in APDL template: {template_path}")
          # 解析 YAML 并填入模板参数
          import yaml
          with open(yaml_file, "r", encoding="utf-8") as f:
              yaml_data = yaml.safe_load(f) or {}
          
          part = yaml_data.get("Parts", [{}])[0]
          material = yaml_data.get("Materials", [{}])[0]
          solver = yaml_data.get("Solver", {})
          bcs = yaml_data.get("BoundaryConditions", [])
          
          # 热源参数提取
          flux_bc = next((bc for bc in bcs if bc.get("Type", "").upper() == "FLUX"), {})
          temp_bc = next((bc for bc in bcs if bc.get("Type", "").upper() in {"T", "TEMP"}), {})
          
          flux_box = flux_bc.get("Box", [0,0,0,0,0,0])
          temp_box = temp_bc.get("Box", [0,0,0,0,0,0])
          
          load_steps = solver.get("LoadSteps", [{}])
          num_substeps = load_steps[0].get("NumSubsteps", 20) if load_steps else 20
          
          with open(template_path, "r", encoding="utf-8") as ft:
              macro_content = ft.read()
              
          # 参数替换
          replacements = {
              "MAT_K": str(material.get("Conductivity", 45.0)),
              "MAT_RHO": str(material.get("Density", 7.85e-9)),
              "MAT_C": str(material.get("HeatCapacity", 4.6e8)),
              "PART_X1": "0.0",
              "PART_X2": str(part.get("Length", 5.0)),
              "PART_Y1": "0.0",
              "PART_Y2": str(part.get("Width", 10.0)),
              "PART_DX": str(part.get("dx", 0.1)),
              "BC_FLUX_XMIN": str(flux_box[0]),
              "BC_FLUX_XMAX": str(flux_box[1]),
              "BC_FLUX_VAL": str(flux_bc.get("Value", 100.0)),
              "BC_TEMP_XMIN": str(temp_box[0]),
              "BC_TEMP_XMAX": str(temp_box[1]),
              "BC_TEMP_VAL": str(temp_bc.get("Value", 100.0)),
              "SOL_SUBSTEPS": str(num_substeps),
              "SAMPLE_START_X": str(start_x),
              "SAMPLE_START_Y": str(start_y),
              "SAMPLE_END_X": str(end_x),
              "SAMPLE_END_Y": str(end_y),
          }
          
          for k, v in replacements.items():
              macro_content = macro_content.replace(k, v)
              
          with open(mac_file, "w", encoding="utf-8") as fm:
              fm.write(macro_content)
      else:
          # 退回到由 generator 动态拼接拼凑 APDL
          _generate_apdl_from_yaml(
              yaml_file,
              mac_file,
              job_name=job_name,
              result_substep=substep,
              default_start_x=start_x,
              default_end_x=end_x,
              default_start_y=start_y,
              default_end_y=end_y if end_y != 0.0 else None,
          )

      parameters: dict[str, Any] = {"START_X": start_x, "END_X": end_x}
      if start_y != 0.0:
          parameters["START_Y"] = start_y
      if end_y != 0.0:
          parameters["END_Y"] = end_y
      if substep:
          parameters["SUBSTEP"] = substep

      result = RUNNER.run_mac_file(mac_file, work_dir, job_name, parameters=parameters)
      ansys_txt_file = os.path.join(
          work_dir,
          f"ansys_val_results_sub{substep}.txt" if substep else "ansys_val_results.txt",
      )
      db_file = os.path.join(work_dir, f"{job_name}.db")
      out_file = os.path.join(work_dir, f"{job_name}.out")
      err_file = os.path.join(work_dir, f"{job_name}.err")

      text_result = ResultParser.parse_prvar_output(ansys_txt_file)
      txt_ok = bool(text_result.get("success"))
      db_exists = os.path.exists(db_file)
      has_fatal_error = "*** ERROR ***" in (result.error_log or "")
      success = not has_fatal_error and txt_ok and db_exists

      warnings: list[str] = []
      if not result.success:
          warnings.append(result.message)
      if not db_exists:
          warnings.append(f"ANSYS database file was not found: {db_file}")
      if not txt_ok:
          warnings.append(
              text_result.get(
                  "message",
                  f"ANSYS text result was not parseable: {ansys_txt_file}",
              )
          )

      res_dict = {
          "success": success,
          "work_dir": work_dir,
          "mac_file": mac_file,
          "ansys_txt_file": ansys_txt_file,
          "db_file": db_file,
          "db_exists": db_exists,
          "out_file": out_file,
          "err_file": err_file,
          "result_files": result.result_files,
          "elapsed_seconds": result.elapsed_seconds,
          "message": result.message,
          "warnings": warnings,
          "error_log_tail": result.error_log[-2000:] if result.error_log else "",
          "text_result": text_result,
      }

      try:
          save_ansys_validation_to_db(DB_FILE, res_dict, yaml_file, substep, job_name)
      except Exception as db_err:
          res_dict["db_warning"] = f"Failed to write SQLite database: {db_err}"

      return res_dict
  ```

- [x] **Step 2: 修改 server.py 的 FastMCP facade 接口中暴露并透传参数**
  在 `run_ansys_yaml_case` 装饰的工具接口中，新增参数 `template_name: str = ""`，并透传。
  修改如下：
  ```python
  @mcp.tool()
  def run_ansys_yaml_case(
      yaml_file: str,
      work_dir: str = "",
      substep: int = 0,
      start_x: float = 1.0,
      end_x: float = 1.0,
      start_y: float = 0.0,
      end_y: float = 0.0,
      job_name: str = "ansys_smoke_test",
      template_name: str = "",
  ) -> dict:
      """由 GRPD PD.yaml 生成 APDL，运行 ANSYS，并返回 txt/db/out/err 等结果路径。"""
      return _run_ansys_yaml_case(
          yaml_file,
          work_dir,
          substep,
          start_x,
          end_x,
          start_y,
          end_y,
          job_name,
          template_name,
      )
  ```

---

## 验证计划 (Verification Plan)

### 自动化验证步骤
1.  **运行内置模板测试**：
    在 MCP 的原生隔离目录中，指定参数 `template_name="thermal_validation"` 运行 `ansys-server.run_ansys_yaml_case` 求解 `Thermal_Validation_MCP/PD.yaml` 算例。
    验证：
    * 产生的工作目录处于 `ansys-mcp-server/work_dir/run_XXXX` 目录下。
    * APDL 宏文件成功使用了内置 `thermal_validation.mac`，并且材料、几何及边界变量占位符已被正确渲染替换。
    * 对标结果中的左端点温度是否完美契合一维导热解析解 $111.111\text{ K}$，且云图等温线无任何角度弯曲。
2.  **运行自动拼接（无模板）测试**：
    不指定 `template_name`（或传空字符串），再次调用此工具对标该算例，验证退回到使用修改后的 `generator.py`（带有 `LSEL`/`SFL` 跨度自适应推导的边界施加），输出的温度和曲线是否同样完美契合一维解，以验证自动拼接的鲁棒性。
