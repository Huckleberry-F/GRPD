---
name: material-model-implementer
description: "GRPD 材料本构专项入口。用于新增、修改或评审 Material/MechanicalMaterial/ThermalMaterial 子类、J2/JC/热传导材料、状态变量 allocateStateVariables/bindStateVariables/commitState、材料 YAML 参数、MaterialRegistry 注册和材料 CMake 集成。"
---

# GRPD 材料本构入口

使用中心工具包：`../grpd-cae-toolkit`。

## 必做流程

1. 读取 `../grpd-cae-toolkit/references/material-state.md`。
2. 读取 `../grpd-cae-toolkit/references/registry-factory.md`。
3. 若新增 `.cpp`，运行 `python .agents/skills/superCAE/skills/grpd-cae-toolkit/scripts/check_cmake_sources.py .`。
4. 若涉及字段或状态变量，运行 `python .agents/skills/superCAE/skills/grpd-cae-toolkit/scripts/check_field_references.py .`。

## 判断重点

- 材料参数解析、物理范围验证、状态变量分配、Old/Trial swap、指针绑定和 commit 是否闭合。
- 注册名是否匹配 YAML `Materials.Type`。
- 新增源文件是否进入 `PDCommon/Material/CMakeLists.txt`。
