# GRPD 仓库无用文件清理与文档归类设计方案 (Design Spec)

**日期**：2026-06-16
**作者**：Antigravity
**状态**：已批准 (根据用户最新反馈：删除根目录所有入口路由脚本，直接在 `setup/` 子目录下手动运行即可)

---

## 1. 目标 (Goal)
为了保持 GRPD 仓库的清爽，优化目录结构，提升仓库管理效率，本方案旨在：
- 清理根目录下所有废弃的历史临时替换脚本、备份文件、运行日志。
- 清理根目录下不再需要的入口路由脚本（环境配置和编译编译引导完全转入 `setup/` 文件夹中进行，保持根目录绝对整洁）。
- 将有价值的参考文献、YAML 速查字典、历史架构设计文档分别归类到 `docs/` 对应的子目录中。
- 物理删除并用 `.gitignore` 忽略被误提交到 Git 中的 Doxygen 生成物（`docs/html/` 和 `docs/latex/`），以缩减仓库冗余文件数量（减少 2300 多个文件）。

---

## 2. 清理与归类方案设计 (Design)

### 2.1 彻底删除的文件（22 项）
以下文件属于开发过程中的临时脚本、历史残留或已废弃的大文件，将直接通过 `git rm` 或物理删除：
- **临时 Python/Shell/Powershell 脚本（12 个）**：
  - `replace.py` (字符替换)
  - `replace_cmake.py` (CMake 参数替换)
  - `replace_engine.py` (求解引擎替换)
  - `rename_fracture.py` (损伤断裂命名更改)
  - `revert_zhang.py` (Zhang稳定器还原)
  - `update_yaml.py` (YAML 格式更新)
  - `fix_guards.ps1` (修复 C++ 头文件 guard)
  - `export_ai_context.py` (打包 AI 上下文)
  - `script.py` (临时杂项脚本)
  - `setup_build.sh` (根目录编译入口路由，用户选择删除)
  - `setup_build.cmd` (根目录编译入口路由，用户选择删除)
  - `setup_env.cmd` (根目录环境入口路由，用户选择删除)
  - `setup_env.ps1` (旧版环境配置脚本，用户选择删除)
- **过期的冗余上下文与临时生成物（3 个）**：
  - `GRPD_AI_Context.md` (718KB 的陈旧 AI 拼接上下文，已无用)
  - `GEMINI.md` (Gemini 缓存日志文件)
  - `Generate_PPT_Diagrams.html` (PPT 临时图表网页)
- **配置文件、备份与日志（5 个）**：
  - `Doxyfile` (Doxygen 配置文件，用户选择删除)
  - `Doxyfile.bak` (Doxygen 配置备份)
  - `compile_flags.txt.bak` (编译参数备份)
  - `doxygen_warnings.log` (Doxygen 警告日志)
  - `test.log` (测试运行日志)

### 2.2 彻底删除并加入 Git 忽略的 Doxygen 输出文档
- **待删除的跟踪目录**：`docs/html/` 和 `docs/latex/`（包含 2300 多个自动生成文件）。
- **待修改的忽略规则**：在根目录 `.gitignore` 中追加对 Doxygen 自动生成路径的过滤规则：
  ```gitignore
  # === Doxygen 自动生成文档 ===
  docs/html/
  docs/latex/
  ```

### 2.3 移动并归类的文档与资料（5 项）
为了保持根目录绝对整洁，对有保留价值的文档和参考资料做如下重组：
- **参考论文归类**：
  - 将根目录下的近场动力学论文 `osti_2566435.pdf` 移动到新建的归类文件夹中：`docs/references/osti_2566435.pdf`。
- **YAML 字典归类**：
  - 将根目录下的参数速查字典 `GRPD_YAML_Dictionary.md` 移动到文档主目录：`docs/GRPD_YAML_Dictionary.md`。
- **架构说明归类**：
  - 将根目录下的 `架构说明_PPT.md` 移动到文档主目录：`docs/架构说明_PPT.md`。
- **历史计划归类**：
  - 将根目录下的 `Contact_V5_Architecture_Plan.md` 和 `Fracture_IMEX_Plan.md` 移动到 superpowers 历史计划存档路径：
    - `docs/superpowers/plans/archive/Contact_V5_Architecture_Plan.md`
    - `docs/superpowers/plans/archive/Fracture_IMEX_Plan.md`

---

## 3. 自我审查 (Self-Review)
1. **Doxygen 依赖性检查**：删除 `docs/html/`、`docs/latex/` 和主配置文件 `Doxyfile` 并不影响项目核心的编译与求解器运行。若后续需要重新生成文档，只需使用 `doxygen -g` 重新创建配置文件即可。并且因 `.gitignore` 的存在，本地文档生成不会再次污染 Git 变动状态。
2. **Setup 独立性检查**：已确认 `setup/` 各个子文件夹下的 shell/powershell 脚本（如 `setup/mac/setup_and_build.sh`、`setup/win/install_deps.ps1`）在执行时使用了相对于脚本自身的路径或通过 `dirname` 获取根目录，均不依赖根目录下的入口路由文件，删除根目录路由脚本不影响编译与配环境功能。
3. **语言锁定检查**：方案、规格说明及注释锁死为中文。

---

## 4. 后续执行建议
经用户同意，本方案将直接转入编写详细的物理执行计划书（`docs/superpowers/plans/2026-06-16-repository-cleanup-plan.md`）进行单流执行。
