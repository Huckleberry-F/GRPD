---
name: numerical-test-generator
description: "GRPD 数值验证专项入口。用于生成或设计回归测试、解析解对比、材料单点测试、patch test、热传导验证、接触/断裂最小算例、输出字段检查和构建运行验证流程。"
---

# GRPD 数值验证入口

使用中心工具包：`../grpd-cae-toolkit`。

## 必做流程

1. 读取 `references/validation-playbook.md`。
2. 根据改动类型读取材料、字段、求解循环、输入输出或邻域对应 reference。
3. 如验证 PD.yaml，运行 `python .agents/skills/superCAE/skills/grpd-cae-toolkit/scripts/validate_pd_yaml_basic.py <PD.yaml>`。
4. 如验证输出字段，运行 `python .agents/skills/superCAE/skills/grpd-cae-toolkit/scripts/inspect_writer_variables.py . --yaml <PD.yaml>`。

## 判断重点

- 每个改动至少设计一个小算例。
- 说明运行命令、期望结果、失败判据和是否需要人工看 VTK/日志。
- 大算例只能作为补充回归，不能替代最小验证。
- 如果设计参数扫描或多次对比实验，同步使用 `../grpd-experiment-tracker` 记录参数、收敛状态、误差指标和结果路径。
