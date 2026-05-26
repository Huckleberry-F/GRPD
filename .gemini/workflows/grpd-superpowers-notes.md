---
description: "GRPD 中使用 Superpowers 与 Antigravity workflow 的项目级优先级说明。"
---

# GRPD Superpowers Notes

本项目采用项目级 `.gemini` 方式试装原版 Superpowers，不使用全局 Antigravity profile，也不默认生成 `.agent/`。

## 优先级

1. 根目录 `AGENTS.md` 是 GRPD 仓库的最高项目规则。
2. `.gemini/skills/*` 中的 GRPD 专项 skill 负责 CAE/C++/数值验证等领域规则。
3. `.gemini/skills/superpowers/*` 提供通用软件工程流程纪律。
4. `.gemini/workflows/*` 只作为 Antigravity 调用入口，不覆盖 GRPD 专项规则。

## 使用建议

- 新功能设计：先用 `brainstorm`，再用 `write-plan`。
- 已批准计划：用 `execute-plan`。
- 行为变更或 bugfix：优先用 `tdd` 或 `debug`。
- 分支收尾：用 `finish-branch`，并额外检查子模块、构建与数值验证。
