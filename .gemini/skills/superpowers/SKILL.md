---
name: superpowers
description: "Superpowers 通用开发流程入口。用于 brainstorming、planning、TDD、debugging、review、finish-branch 等结构化软件工程工作流；在 GRPD 中必须服从根目录 AGENTS.md 与 .gemini/skills 下的 CAE 专项规则。"
---

# Superpowers for GRPD

本目录保存原版 `obra/superpowers` 的核心 skill 内容，作为 Antigravity 项目级工作流的本地参考。

## 使用原则

- 需要需求澄清、计划编写、TDD、系统化调试、代码评审或分支收尾时，优先通过 `.gemini/workflows/*.md` 入口调用对应 workflow。
- 涉及 GRPD 的 C++17、CMake、OpenMP、材料本构、物理场、求解循环、后处理或数值验证时，根目录 `AGENTS.md` 与 `.gemini/skills/*` 中的 GRPD 专项 skill 优先级更高。
- 原版 Superpowers 中涉及并行子代理、Claude 专用工具或平台特定命令的内容，在 Antigravity 中按顺序单流执行，并映射到当前 IDE 可用工具。

## 本地路径

- 原版 skill 文件位于：`.gemini/skills/superpowers/skills/<skill-name>/SKILL.md`
- GRPD workflow 入口位于：`.gemini/workflows/*.md`
