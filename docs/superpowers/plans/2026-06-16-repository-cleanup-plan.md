# 仓库清理与文档归类执行计划 (Implementation Plan)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 清理 GRPD 仓库根目录下的冗余脚本与日志，删除误提交的 Doxygen 网页，并对重要的文献及参数字典进行归类整理，使仓库结构清爽规范。

**Architecture:** 物理删除与 `git rm` 历史无用文件，通过新建 `docs/references/` 目录和利用现有的 `docs/superpowers/plans/archive/` 进行分类迁移，同时更新 `.gitignore` 彻底屏蔽 Doxygen 输出。

**Tech Stack:** Git, Shell Commands

---

### Task 1: 创建归类所需的新文件夹

**Files:**
- Create: `docs/references/` (新目录，无版本控制，后续存放文献)
- Create: `docs/superpowers/plans/archive/` (新目录，用于归档历史计划)

- [ ] **Step 1: 物理创建目标目录**

运行：
```bash
mkdir -p docs/references
mkdir -p docs/superpowers/plans/archive
```

---

### Task 2: 归类整理保留文件

**Files:**
- Modify: `osti_2566435.pdf` $\rightarrow$ `docs/references/osti_2566435.pdf`
- Modify: `GRPD_YAML_Dictionary.md` $\rightarrow$ `docs/GRPD_YAML_Dictionary.md`
- Modify: `架构说明_PPT.md` $\rightarrow$ `docs/架构说明_PPT.md`
- Modify: `Contact_V5_Architecture_Plan.md` $\rightarrow$ `docs/superpowers/plans/archive/Contact_V5_Architecture_Plan.md`
- Modify: `Fracture_IMEX_Plan.md` $\rightarrow$ `docs/superpowers/plans/archive/Fracture_IMEX_Plan.md`

- [ ] **Step 1: 移动文献 PDF**

运行：
```bash
git mv osti_2566435.pdf docs/references/osti_2566435.pdf
```

- [ ] **Step 2: 移动 YAML 字段速查字典**

运行：
```bash
git mv GRPD_YAML_Dictionary.md docs/GRPD_YAML_Dictionary.md
```

- [ ] **Step 3: 移动架构说明 PPT**

运行：
```bash
git mv 架构说明_PPT.md docs/架构说明_PPT.md
```

- [ ] **Step 4: 归档历史开发计划**

运行：
```bash
git mv Contact_V5_Architecture_Plan.md docs/superpowers/plans/archive/Contact_V5_Architecture_Plan.md
git mv Fracture_IMEX_Plan.md docs/superpowers/plans/archive/Fracture_IMEX_Plan.md
```

---

### Task 3: 彻底清理根目录下无用的脚本、冗余大文件与日志

**Files:**
- Delete: `replace.py`
- Delete: `replace_cmake.py`
- Delete: `replace_engine.py`
- Delete: `rename_fracture.py`
- Delete: `revert_zhang.py`
- Delete: `update_yaml.py`
- Delete: `fix_guards.ps1`
- Delete: `export_ai_context.py`
- Delete: `script.py`
- Delete: `setup_build.sh`
- Delete: `setup_build.cmd`
- Delete: `setup_env.cmd`
- Delete: `setup_env.ps1`
- Delete: `GRPD_AI_Context.md`
- Delete: `GEMINI.md`
- Delete: `Generate_PPT_Diagrams.html`
- Delete: `Doxyfile.bak`
- Delete: `compile_flags.txt.bak`
- Delete: `doxygen_warnings.log`
- Delete: `test.log`

- [ ] **Step 1: 使用 git rm 彻底清理并移除版本控制的脚本与文件**

运行：
```bash
git rm replace.py replace_cmake.py replace_engine.py rename_fracture.py revert_zhang.py update_yaml.py fix_guards.ps1 export_ai_context.py script.py setup_build.sh setup_build.cmd setup_env.cmd setup_env.ps1 GRPD_AI_Context.md Generate_PPT_Diagrams.html
```

- [ ] **Step 2: 物理删除未跟踪的临时日志与备份文件**

运行：
```bash
rm -f test.log doxygen_warnings.log Doxyfile.bak compile_flags.txt.bak GEMINI.md
```

---

### Task 4: 清理 Doxygen 网页输出并更新忽略规则

**Files:**
- Delete: `docs/html/` (包含约 1542 个生成文件)
- Delete: `docs/latex/` (包含约 806 个生成文件)
- Modify: `.gitignore`

- [ ] **Step 1: 从 Git 仓库中物理移除已跟踪的 Doxygen 生成文件目录**

运行：
```bash
git rm -r -f docs/html docs/latex
```

- [ ] **Step 2: 在 `.gitignore` 中追加对 Doxygen 自动生成路径的屏蔽**

修改: `[.gitignore](file:///Users/hanbozhang/C++/GRPD/.gitignore)`
在文件末尾追加：
```gitignore

# === Doxygen 自动生成文档 ===
docs/html/
docs/latex/
```

---

### Task 5: 提交更改并验证状态

- [ ] **Step 1: 验证 git 状态**

运行：
```bash
git status
```
期待输出：所有被清理的文件都处于已删除/已移动状态，且 `.gitignore` 修改完成，没有多余的 untracked 文件。

- [ ] **Step 2: 提交清理与重构提交**

运行：
```bash
git commit -m "refactor: cleanup redundant scripts and archives, reorganize docs and ignore doxygen output"
```
