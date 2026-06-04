---
name: execute-single-flow
description: "在 Antigravity 下按单流模式执行已批准计划，并为 GRPD 任务加载 CAE 路由与验证门禁。"
---

Invoke `.agents/skills/superpowers/skills/single-flow-task-execution/SKILL.md` and follow it exactly.

GRPD override: before each task, invoke `.agents/skills/superCAE/skills/grpd-cae-router/SKILL.md` to choose the smallest necessary specialist skills and validation commands. Keep one active task at a time in `docs/superpowers/task.md`.
