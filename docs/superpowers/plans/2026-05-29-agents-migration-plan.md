# .agents 统一目录大重构与移植计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 GRPD 项目的 AI Skills 体系、Workflows、本地测试及 MCP Server 配置从旧有的 `.gemini/` 彻底迁移至官方统一的 `.agents/` 目录，修复所有文件内部的路径硬编码，并保证跨平台与跨 IDE 的高可移植性。

**Architecture:** 物理迁移文件夹，批量更新所有文本文件中的路径引用；配置 `.gitignore` 以正确跟踪技能文件同时保护个人配置；更新 `settings.json` 的路径设计。

**Tech Stack:** Python 3, Git, Shell commands / IDE tool actions.

---

## 核心设计要点与风险控制

1. **Git 忽略与跟踪冲突：**
   当前 `.gitignore` 中写了 `.agents/`，这将导致迁移后的 Skills 和 Workflows 无法提交。必须修改 `.gitignore` 允许跟踪 `.agents/`，但需通过否定忽略规则继续保护本地配置（如 `settings.json` 和实验数据库）。
2. **文本批量路径替换：**
   包括 9 个 `workflows/*.md`、14 个 `superCAE/skills/*/SKILL.md`、主路由、自测脚本及 `README.md`，均有大量对 `.gemini` 路径的硬编码，必须全面更新为 `.agents`。
3. **可移植路径改造：**
   `settings.json.example` 中的 `<GRPD_PROJECT_ROOT_PATH>` 变量维持不变，但需要确保配置文件的示例路径全部前移至 `.agents/`。

---

## 详细实施步骤

### Task 1: 基础设施配置更新 (.gitignore)

**Files:**
- Modify: `.gitignore:22-23`

- [ ] **Step 1: 修改 `.gitignore` 规则**
  将 `.gitignore` 中简单粗暴的 `.agents/` 规则替换为细分规则，确保允许跟踪技能文件，但保护个人本地配置。
  目标内容：
  ```diff
  - .agent/
  - .agents/
  + .agent/
  + # 跟踪 .agents 目录，但忽略本地私有配置与临时数据
  + .agents/settings.json
  + .agents/**/*.sqlite
  + .agents/**/*.sqlite-shm
  + .agents/**/*.sqlite-wal
  ```

- [ ] **Step 2: 验证 git 状态**
  运行：`git status` 确保修改生效且未引发大范围未跟踪文件混乱。

---

### Task 2: 物理文件目录重命名与迁移

**Files:**
- Move: `.gemini/` -> `.agents/` (包含所有子目录)

- [ ] **Step 1: 创建 `.agents` 并迁移数据**
  执行文件夹重命名与转移：
  1. 将 `.gemini/skills/` 移动至 `.agents/skills/`
  2. 将 `.gemini/workflows/` 移动至 `.agents/workflows/`
  3. 将 `.gemini/tests/` 移动至 `.agents/tests/`
  4. 将 `.gemini/settings.json` 移动至 `.agents/settings.json`
  5. 将 `.gemini/settings.json.example` 移动至 `.agents/settings.json.example`
  6. 将 `.gemini/README.md` 移动至 `.agents/README.md`

- [ ] **Step 2: 清理旧的目录**
  确保旧的 `.gemini/` 文件夹已被完全清除。

---

### Task 3: 批量更新 Workflows 路径引用

**Files:**
- Modify: `.agents/workflows/brainstorm.md`
- Modify: `.agents/workflows/debug.md`
- Modify: `.agents/workflows/execute-plan.md`
- Modify: `.agents/workflows/execute-single-flow.md`
- Modify: `.agents/workflows/finish-branch.md`
- Modify: `.agents/workflows/grpd-route.md`
- Modify: `.agents/workflows/grpd-superpowers-notes.md`
- Modify: `.agents/workflows/tdd.md`
- Modify: `.agents/workflows/write-plan.md`

- [ ] **Step 1: 修改所有入口 workflow 文件中的路径**
  将所有 `.gemini/skills/` 和 `.gemini/workflows/` 的文本引用，统一替换为 `.agents/skills/` 和 `.agents/workflows/`。

---

### Task 4: 更新 Skills 内部引用与脚本调用

**Files:**
- Modify: `.agents/skills/superpowers/SKILL.md`
- Modify: `.agents/skills/superCAE/SKILL.md`
- Modify: 所有存在 `.gemini` 硬编码的专项 SKILL.md 文件（重点是调用 Python 工具的那些行）

- [ ] **Step 1: 更新 superpowers 及 superCAE 根级指引**
  修改 `.agents/skills/superpowers/SKILL.md` 和 `.agents/skills/superCAE/SKILL.md`，将路径声明更新为 `.agents/`。

- [ ] **Step 2: 批量更新专项 Skill 中的脚本调用路径**
  将各专项 Skill.md（如 `solver-loop-safety-reviewer`、`postprocess-exporter`、`physics-field-pipeline-auditor`、`openmp-kernel-optimizer` 等）中执行检测脚本的路径从 `python .gemini/...` 替换为 `python .agents/...`。

---

### Task 5: 验证与辅助文件更新

**Files:**
- Modify: `.agents/tests/check_grpd_antigravity_overlay.py`
- Modify: `.agents/README.md`

- [ ] **Step 1: 修改自测脚本**
  更新 `check_grpd_antigravity_overlay.py` 中 `REQUIRED_FILES` 的路径映射，将 `.gemini/` 全面替换为 `.agents/`。
  
- [ ] **Step 2: 修改说明文档**
  将 `.agents/README.md` 的介绍文本与 Mermaid 图表中的路径说明更新为新版规范。

---

### Task 6: 模板配置更新与本地验证

**Files:**
- Modify: `.agents/settings.json.example`
- Modify: `.agents/settings.json`

- [ ] **Step 1: 更新示例模板与本地 settings.json**
  确保 `settings.json.example` 中所有的 `<GRPD_PROJECT_ROOT_PATH>` 依然指向 `.agents/` 目录结构。更新本地 `.agents/settings.json` 中的 `cwd` 与 `args` 路径。

- [ ] **Step 2: 运行测试检查脚本**
  执行验证：`python .agents/tests/check_grpd_antigravity_overlay.py`
  Expected: 输出 Passed 且 Fail 为 0。
