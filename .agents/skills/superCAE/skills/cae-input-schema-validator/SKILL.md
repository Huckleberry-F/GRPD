---
name: cae-input-schema-validator
description: "GRPD YAML 输入专项入口。用于生成、检查或修复 PD.yaml、Init*.cpp 解析逻辑、材料/求解器/边界条件/接触/输出字段、默认值、必填项、单位量纲、错误消息和 GRPD_YAML_Dictionary.md 文档一致性。"
---

# GRPD YAML Schema 入口

使用中心工具包：`../grpd-cae-toolkit`。

## 必做流程

1. 读取 `../grpd-cae-toolkit/references/input-output-schema.md`。
2. 对目标输入文件运行 `python .agents/skills/superCAE/skills/grpd-cae-toolkit/scripts/validate_pd_yaml_basic.py <PD.yaml>`。
3. 如涉及输出字段，运行 `python .agents/skills/superCAE/skills/grpd-cae-toolkit/scripts/inspect_writer_variables.py . --yaml <PD.yaml>`。
4. 如涉及注册名，运行 `python .agents/skills/superCAE/skills/grpd-cae-toolkit/scripts/list_registry_macros.py .`。

## 判断重点

- 真实读取逻辑以 C++ 代码为准，文档次之。
- 修改字段时同步更新示例和 `GRPD_YAML_Dictionary.md`。
- 必填字段缺失应给出明确错误。
