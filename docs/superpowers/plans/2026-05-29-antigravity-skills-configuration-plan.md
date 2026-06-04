# Antigravity Skills Configuration Investigation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 检测并梳理 GRPD 仓库中现有的本地 AI Skills 体系及其具体路径，并研究如何在 Antigravity IDE 环境下完成有效适配与激活。

**Architecture:** 
1. 遍历系统默认的插件路径与工作区的 `.gemini/skills/` 目录。
2. 读取关键的 `AGENTS.md` 和各核心技能的 `SKILL.md`，提取架构与执行规约。
3. 整理出一套能够在无原生 `activate_skill` 接口下运转的适配方案（Plan C），并输出最终总结报告。

**Tech Stack:** 目录检索, Markdown 报告生成

---

### Task 1: 扫描并列举本地技能目录

**Files:**
- Read: `.gemini/skills/superCAE/` 目录
- Read: `.gemini/skills/superpowers/` 目录

- [x] **Step 1: 检索 superCAE 业务技能**
遍历工程中的计算层规范技能，确认诸如本构、求解器、内核算法相关的 AI 提示词配置文件。

- [x] **Step 2: 检索 superpowers 流程技能**
遍历通用的工作流管控技能，诸如写计划、执行代码、测试驱动开发等规范。

### Task 2: 提取 Antigravity 环境的适配约束

**Files:**
- Read: `AGENTS.md`

- [x] **Step 1: 阅读全局代理规约**
解析 `AGENTS.md` 中的终极工作流拦截机制 (Anti-Planning-Mode) 和物理落盘规约。

- [x] **Step 2: 制定 Plan C (物理加载方案)**
明确在无原生调用权限时，通过 `view_file` 物理读取 `SKILL.md` 并使用 `[Active Skill: xxx]` 宣告的变通加载策略。

### Task 3: 撰写配置总结报告

**Files:**
- Create: `docs/superpowers/plans/2026-05-29-antigravity-skills-configuration-report.md`

- [x] **Step 1: 总结系统架构与方案**
将任务1与任务2的发现汇总，以物理文件的形式落盘一份供后续会话与 Codex 协作的规范参考文档。
