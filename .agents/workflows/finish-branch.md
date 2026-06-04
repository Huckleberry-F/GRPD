---
name: finish-branch
description: "完成开发分支收尾：检查 diff、运行验证、整理提交、push，并准备 PR。"
---

Invoke the `.agents/skills/superpowers/skills/finishing-a-development-branch/SKILL.md` workflow and follow it exactly.

GRPD override: before commit or push, check `git status`, inspect submodule changes, review the staged diff, and run the GRPD smoke test (`/grpd-smoke-test`) to prove the touched subsystem still works.
