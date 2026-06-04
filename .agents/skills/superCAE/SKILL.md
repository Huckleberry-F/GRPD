---
name: superCAE
description: "GRPD CAE 专项技能集合入口。用于材料本构、求解循环、OpenMP Kernel、网格邻域、YAML 输入、VTK 后处理、CMake、数值验证、ANSYS/MATLAB 对标和实验追踪。"
---

# SuperCAE for GRPD

`superCAE` 是 GRPD 的 CAE 领域技能集合，与 `superpowers` 同级：

- `superpowers` 负责通用软件工程流程。
- `superCAE` 负责 GRPD/CAE 专项判断、物理约束和验证策略。

## 使用原则

- 遇到 GRPD C++、材料、求解器、Kernel、OpenMP、输入输出或数值验证任务时，先读取 `skills/grpd-cae-router/SKILL.md`。
- `grpd-cae-router` 只做路由，不替代专项 skill。
- 每个专项 skill 位于：`.agents/skills/superCAE/skills/<skill-name>/SKILL.md`。
- 与 Superpowers 联合使用时，由 Superpowers 管计划和执行节奏，由 SuperCAE 管 CAE 风险和验证证据。

## 推荐入口

- CAE 路由：`.agents/workflows/grpd-route.md`
- Antigravity 单流执行：`.agents/workflows/execute-single-flow.md`
- Overlay 配置检查：`python .agents/tests/check_grpd_antigravity_overlay.py`
