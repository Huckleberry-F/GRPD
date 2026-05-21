---
name: grpd-smoke-tester
description: "GRPD 自动化冒烟测试与验证入口。用于在修改 C++ 源码（如本构模型、积分核）后，一键执行全流程验证：驱动 GRPD 编译求解、调度 ansys-server 提取基准解、调用 path-comparison-server 进行数据切片融合与误差图表生成。"
---

# GRPD 自动化冒烟测试 (Smoke Test)

默认走快速固定流程。除非 MCP 报错、用户要求排查源码，或缺少必要参数，不要扫描全仓库、不要读取无关 skill、不要临时生成 Python 编排脚本。

## 必做流程

1. 只读取 `references/smoke-test-playbook.md`；不要先读 `../grpd-cae-toolkit` 或其他 reference，除非失败后需要定位原因。
2. 直接按职责隔离顺序调用三个 MCP；不要新建 `run_*.py`、`*_workflow.py` 或其他临时脚本：
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
       work_dir="<Ansys_Work_Dir>",
       substep=<Substep_Num>
       start_x=<Start_X_Coordinate>,
       end_x=<End_X_Coordinate>
   )

   path-comparison-server.generate_comparison_report(
       vtk_file="<GRPD_VTK>",
       ansys_txt_file="<ANSYS_TXT>",
       output_dir="<Comparison_Output_Dir>",
       start_x=<Start_X_Coordinate>,
       end_x=<End_X_Coordinate>
   )
   ```
   > [!NOTE]
   > 如果用户指定了既有 `vtk_file`，跳过 GRPD 编译和运行；否则由 `grpd-server` 运行 GRPD 并定位目标 VTK。ANSYS 阶段必须生成 `.db` 文件，供人工复查。
3. 如果三个 MCP 都成功，将输出的对比图、Excel、summary、zip 路径以及最终误差百分比展示给用户，并宣告 **[PASS]**。
4. 如果 MCP 返回失败，先报告失败阶段和返回消息；只有在需要定位根因时，才读取相关源码、APDL、日志或 YAML。
5. 如果误差过大或发生数值崩溃，宣告 **[FAIL]**，再结合必要的 C++ 本构或 Kernel 改动分析原因。
6. 如果用户正在做参数扫描或要求保留实验历史，调用 `grpd-experiment-server` 将本次运行的参数、误差、日志、VTK、ANSYS db、图表和报告路径入库。

## 判断重点

- 未指定 `vtk_file` 时，务必确保最新编译的 `GRPD.exe` 被实际执行，防止读取旧的缓存数据。
- 传入 `--start-x` 坐标时，需确保该坐标处于几何物理范围内。
- ANSYS 阶段返回的 `db_file` 用于 GUI/人工复查；若缺失，应作为 warning 或失败原因向用户说明。
- 禁止为了冒烟测试临时创建 Python runner；现有 MCP 已覆盖完整流程。
