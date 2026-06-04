---
name: grpd-smoke-tester
description: "GRPD 自动化冒烟测试与验证入口。用于在修改 C++ 源码（如本构模型、积分核）后，一键执行全流程验证：驱动 GRPD 编译求解、调度 ansys-server 提取基准解，并通过 grpd-validation-server.compare_grpd_vtk_with_ansys_txt 进行数据切片融合与误差图表生成。"
---

# GRPD 自动化冒烟测试 (Smoke Test)

默认走快速固定流程。除非 MCP 报错、用户要求排查源码，或缺少必要参数，不要扫描全仓库、不要读取无关 skill、不要临时生成 Python 编排脚本。

## 必做流程

1. 只读取 `references/smoke-test-playbook.md`；不要先读 `../grpd-cae-toolkit` 或其他 reference，除非失败后需要定位原因。
2. 直接按职责隔离顺序调用 MCP Native 工具；不要新建 `run_*.py`、`*_workflow.py` 或其他临时脚本：
   ```text
   grpd-server.get_grpd_vtk_result(
       case_dir="<GRPD_Case_Dir>",
   ```text
   grpd-server.get_grpd_vtk_result(
       case_dir="<GRPD_Case_Dir>",
       vtk_file="<Optional_Existing_VTK>",
       build_dir="<Optional_GRPD_Build_Dir>",
       executable="<Optional_GRPD_Exe>",
       substep=<Substep_Num>
   )

   ansys-server.run_ansys_yaml_case(
       yaml_file="<GRPD_Case_Dir>/PD.yaml",
       work_dir="",  # 留空以使用默认的 MCP 隔离工作目录
       substep=<Substep_Num>
       start_x=<Start_X_Coordinate>,
       end_x=<End_X_Coordinate>
   )

   grpd-validation-server.compare_grpd_vtk_with_ansys_txt(
       vtk_file="<GRPD_VTK>",
       ansys_txt_file="<ANSYS_TXT>",
       output_dir="<Validation_Work_Dir>",  # 建议传入独立验证输出目录；为空则由 validation MCP 自动分配
       start_x=<Start_X_Coordinate>,
       start_y=<Start_Y_Coordinate>,
       start_z=<Start_Z_Coordinate>,
       end_x=<End_X_Coordinate>,
       end_y=<End_Y_Coordinate>,
       end_z=<End_Z_Coordinate>,
       physics_type="<Mechanical|Thermal>",
       components=["UX", "UY", "SEQV"],  # 动态指定待对标的物理场分量列表
       tol=<Optional_Adaptive_Tol>        # 粒子过滤的垂直距离容差 (mm)，不传则自适应
   )
   ```
   > [!NOTE]
   > 如果用户指定了既有 `vtk_file`，跳过 GRPD 编译和运行；否则由 `grpd-server` 运行 GRPD 并定位目标 VTK。ANSYS 阶段必须生成 `.db` 文件，供人工复查。
   > [!TIP]
   > 为了避免文件锁冲突和对标历史覆盖，调用 ansys-server 时，work_dir 参数必须保持留空（即为 ""），从而让服务器自动在 mcp/ansys-mcp-server/work_dir/ 下生成递增序号的 run_XXXX 隔离沙箱。
3. 如果三步 MCP Native 调用都成功，将输出的对比图、Excel、summary、zip 路径以及最终误差百分比展示给用户，并宣告 **[PASS]**。
4. 如果 MCP 返回失败，先报告失败阶段和返回消息；只有在需要定位根因时，才读取相关源码、APDL、日志或 YAML。
5. 如果误差过大或发生数值崩溃，宣告 **[FAIL]**，再结合必要的 C++ 本构或 Kernel 改动分析原因。
6. **【强制入库规约】** 仿真与对标分析结束后，必须自动调用 `grpd-experiment-server` 进行实验数据统一入库：
   - 依次调用 `create_experiment_run` 创建运行，将物理/几何参数（网格大小 dx、长宽厚度、材料导热/力学常数、载荷与边界物理量、采样路径起止坐标）归档入 `parameters` 字典中。
   - 调用 `finish_experiment_run` 写入最终完成状态 `COMPLETED`（或失败状态 `FAILED`），并填入 GRPD 的求解器计算时间 `elapsed_seconds` 和步数 `num_steps`。
   - 调用 `add_experiment_metric` 录入其他性能与对标指标，如 ANSYS 仿真运行耗时 `ansys_elapsed_seconds` 以及温度/位移/应力等对比分量的最大对标相对误差（如 `max_error_temp_percent`、`max_error_uy_percent`、`max_error_seqv_percent`）。
   - 调用 `add_experiment_artifact` 关联物理结果文件，如计算 VTK 文件、对比图表 Plot PNG 文件、Excel 详细对标报告等。

- 未指定 `vtk_file` 时，务必确保最新编译的 `GRPD.exe` 被实际执行，防止读取旧的缓存数据。
- 传入 `--start-x` 坐标时，需确保该坐标处于几何物理范围内。
- ANSYS 阶段返回的 `db_file` 用于 GUI/人工复查；若缺失，应作为 warning 或失败原因向用户说明。
- 对比报告只允许通过 `grpd-validation-server.compare_grpd_vtk_with_ansys_txt` 生成；`ansys-server` 不再承担 VTK/TXT 对比职责。
- 禁止为了冒烟测试临时创建 Python runner；现有 MCP 已覆盖完整流程。
