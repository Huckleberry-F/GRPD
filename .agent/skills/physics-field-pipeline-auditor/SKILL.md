---
name: physics-field-pipeline-auditor
description: "GRPD 物理场链路专项入口。用于检查或扩展 PhysicsFieldRegistry、PhysicsFields 子类、MechanicalFields/ThermalFields、FieldManager 字段注册、Solver.Type 到字段自动生成、材料状态场分配，以及字段声明了但未注册或注册了但未使用的问题。"
---

# GRPD 物理场链路入口

使用中心工具包：`../grpd-cae-toolkit`。

## 必做流程

1. 读取 `../grpd-cae-toolkit/references/field-pipeline.md`。
2. 读取 `../grpd-cae-toolkit/references/architecture.md`。
3. 运行 `python .agent/skills/grpd-cae-toolkit/scripts/check_field_references.py .`。
4. 如涉及注册宏，运行 `python .agent/skills/grpd-cae-toolkit/scripts/list_registry_macros.py .`。

## 判断重点

- `Solver.Type -> PhysicsFieldRegistry -> PhysicsFields -> FieldManager -> Kernel/BC/Material/Post` 是否闭合。
- 字段名、维度、创建阶段、resize 阶段和消费位置是否一致。
- 不要绕过注册体系散落创建字段。
