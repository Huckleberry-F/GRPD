---
name: postprocess-exporter
description: "GRPD 后处理专项入口。用于新增或评审 Writer.Variables、PDEnginePost、Outputer、Writer/VtkWriter、VTK legacy FIELD 输出、标量/向量/张量字段、状态变量输出、ParaView 命名、二进制/ASCII 和输出路径管理。"
---

# GRPD 后处理输出入口

使用中心工具包：`../grpd-cae-toolkit`。

## 必做流程

1. 读取 `../grpd-cae-toolkit/references/input-output-schema.md`。
2. 运行 `python .agents/skills/superCAE/skills/grpd-cae-toolkit/scripts/check_field_references.py .`。
3. 对目标 YAML 运行 `python .agents/skills/superCAE/skills/grpd-cae-toolkit/scripts/inspect_writer_variables.py . --yaml <PD.yaml>`。

## 判断重点

- 输出字段必须真实存在于 `FieldManager`。
- 字段维度决定 scalar/vector/tensor 分类。
- 状态变量输出要明确 Old/Trial/committed 语义和 ParaView 名称。
