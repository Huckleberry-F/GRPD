---
name: grpd-superpowers-notes
description: "GRPD 中使用 Superpowers 与 Antigravity workflow 的项目级优先级说明。"
---

# GRPD Superpowers Notes

本项目采用项目级 `.agents` 方式试装原版 Superpowers，不使用全局 Antigravity profile，并默认生成 `.agents/`。

## 优先级

1. 根目录 `AGENTS.md` 是 GRPD 仓库的最高项目规则。
2. `.agents/skills/superCAE/*` 中的 GRPD 专项 skill 负责 CAE/C++/数值验证等领域规则。
3. `.agents/skills/superpowers/*` 提供通用软件工程流程纪律。
4. `.agents/workflows/*` 只作为 Antigravity 调用入口，不覆盖 GRPD 专项规则。

## 使用建议

- 新功能设计：先用 `brainstorm`，再用 `write-plan`。
- 已批准计划：在 Antigravity 下优先用 `execute-single-flow`；其他环境可继续用 `execute-plan`。
- CAE/C++ 任务：先用 `grpd-route` 选择专项 skill、风险字段和验证路径。
- 行为变更或 bugfix：优先用 `tdd` 或 `debug`。
- 分支收尾：用 `finish-branch`，并额外检查子模块、构建与数值验证。
