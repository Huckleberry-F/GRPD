# GRPD Validation MCP 硬迁移设计

## 目标

本期将 GRPD/ANSYS 对比报告能力从 `ansys-mcp-server` 中硬迁移到独立的 `grpd-validation-mcp-server`。迁移后不保留兼容层，`ansys-mcp-server` 不再公开 `generate_comparison_report`。

## 服务边界

- `grpd-mcp-server`：只负责 GRPD 构建、运行和定位 VTK。
- `ansys-mcp-server`：只负责 APDL 生成、MAPDL 执行、ANSYS 文本/日志/数据库路径返回与解析。
- `grpd-validation-mcp-server`：只负责 GRPD VTK 与 ANSYS TXT 的路径采样、投影对齐、插值误差、图表、Excel、JSON 与 ZIP 报告。
- `grpd-experiment-mcp-server`：只负责实验记录、指标、artifact、参数扫描和历史查询。

## 调用链

```text
grpd-server.get_grpd_vtk_result
-> ansys-server.run_ansys_yaml_case
-> grpd-validation-server.compare_grpd_vtk_with_ansys_txt
-> grpd-experiment-server.add_experiment_metric / add_experiment_artifact
```

## 公开接口

`grpd-validation-server` 新增唯一一期对比入口：

```text
compare_grpd_vtk_with_ansys_txt(
    vtk_file,
    ansys_txt_file,
    output_dir="",
    start_x=0.5, start_y=0.0, start_z=0.0,
    end_x=0.5, end_y=5.0, end_z=0.0,
    physics_type="Mechanical",
    components=None,
    tol=None
)
```

该接口返回 `success`、`excel_path`、`plot_path`、`summary_path`、`zip_path` 以及各通道最大误差百分比。它不写入 experiment DB。

## 废弃策略

`.agents/run_thermal_smoke_test.py` 这类 Python 中转脚本不再允许作为降级路径。MCP 工具不可用时应报告阻塞，而不是生成持久化 runner 脚本。

## 二期范围

四个 MCP 服务统一整理为 `server.py` facade、`src/` 核心、`tests/` 测试的通用结构属于二期，不混入本期硬迁移。
