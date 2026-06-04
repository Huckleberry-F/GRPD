# GRPD 冒烟测试与验证工作流 (Smoke Test Playbook)

## 核心目标
保证 GRPD 的 C++ 计算核心（如本构模型更新、断裂准则修正、积分核优化）在经历修改后，仍然能在基础 Benchmark (如轴对称拉伸、断裂测试) 上输出与 ANSYS 商业软件对标的精确结果。

## 工具流转
自动化冒烟测试强依赖于 `grpd-server`、`ansys-server` 与 `grpd-validation-server` 三个 MCP 服务器的联合，其中对比分析入口已收敛到 `grpd-validation-server.compare_grpd_vtk_with_ansys_txt`：
1. **GRPD 模块 (`grpd-server`)**：构建/运行 GRPD 并定位 `.vtk`；如果用户指定既有 `.vtk`，则跳过运行。
2. **验证模块 (`ansys-server`)**：根据 `PD.yaml` 生成 APDL、运行 ANSYS、导出 txt，并保存 `.db` 数据库文件供人工复查。
3. **分析模块 (`grpd-validation-server.compare_grpd_vtk_with_ansys_txt`)**：自动滤除无用点云，对齐坐标系，进行插值对比，并生成结果文件夹。

## 快速执行规则
- 不要为冒烟测试临时生成 Python runner、workflow 脚本或批处理脚本。
- 不要扫描全仓库；除非 MCP 返回失败或用户要求排查，否则不读取 MCP 源码。
- 优先直接调用 MCP Native 工具，并把上一步返回的路径传给下一步。
- 只有当缺少 `case_dir`、`PD.yaml`、`vtk_file`、`ansys_workdir`、采样坐标或 substep 时，才停下来询问用户。

## 注意事项
- **文件锁定**：GRPD 的输出是带时间戳的文件夹 `Result_YYYYMMDD_HHMMSS`。测试脚本必须能自适应寻址最新的目录和 `.vtk` 文件。
- **坐标提取边界**：点云数据切片由于没有拓扑关系，在 `grpd-validation-server.compare_grpd_vtk_with_ansys_txt` 底层封装的对比逻辑中通过容差过滤（如 `abs(x - target_x) < tol`）。务必确保截取路径在几何范围内。
- **验证判定标准**：
  - 弹性段位移/应力误差应极小 (`< 0.1%`)。
  - 塑性段应力误差由于积分方式不同，通常容忍度为 `< 5%`。
  - 断裂段判定应观察应力是否突降。

## 一键脚本调用示例
当 AI (Codex) 执行测试任务时，按 MCP 职责顺序调用：
```text
grpd-server.get_grpd_vtk_result(
- **验证判定标准**：
  - 弹性段位移/应力误差应极小 (`< 0.1%`)。
  - 塑性段应力误差由于积分方式不同，通常容忍度为 `< 5%`。
  - 断裂段判定应观察应力是否突降。

## 一键脚本调用示例
当 AI (Codex) 执行测试任务时，按 MCP 职责顺序调用：
```text
grpd-server.get_grpd_vtk_result(
    case_dir="<GRPD_Case_Dir>",
    vtk_file="<Optional_Existing_VTK>",
    build_dir="<Optional_GRPD_Build_Dir>",
    substep=<Substep_Num>
)

ansys-server.run_ansys_yaml_case(
    yaml_file="<GRPD_Case_Dir>/PD.yaml",
    work_dir="",  # 留空以自动分配递增的隔离工作目录
    start_x=<Start_X_Coordinate>,
    end_x=<End_X_Coordinate>,
    substep=<Substep_Num>
)

grpd-validation-server.compare_grpd_vtk_with_ansys_txt(
    vtk_file="<GRPD_VTK>",
    ansys_txt_file="<ANSYS_TXT>",
    output_dir="<Validation_Work_Dir>",
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
