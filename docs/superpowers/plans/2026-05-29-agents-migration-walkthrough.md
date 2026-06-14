# .agents 目录统一大迁移验证总结 (Walkthrough)

**Goal:** 将 GRPD 项目的 AI Skills 体系、Workflows、本地测试及 MCP Server 配置从旧有的 `.gemini/` 彻底迁移至官方统一的 `.agents/` 目录，修复所有文件内部的路径硬编码，并保证跨平台与跨 IDE 的高可移植性。

**Date:** 2026-05-29

---

## 变更内容

### 基础设施与配置
- [x] **`.gitignore`**：移除原有的 `.agents/` 全局忽略规则，调整为仅忽略本地非跟踪配置文件（`.agents/settings.json`）和临时 SQLite 数据库（`*.sqlite`、`*-shm`、`*-wal`），确保技能与工作流文件可被 Git 跟踪。
- [x] **物理目录移动**：将原 `.gemini/` 文件夹整体重命名并迁移至 `.agents/` 下，清理了原有的旧目录。

### 内部路径与引用修复
- [x] **Workflows (共 9 个文件)**：将 `.agents/workflows/` 下所有 `.md` 文件中的路径引用从 `.gemini/` 变更为 `.agents/`。
- [x] **Skills (共 16 个文件)**：更新了根级 Skill 指引（`superpowers` 和 `superCAE`）中的基准路径声明。
- [x] **专项 C++ 验证技能**：更新了 `solver-loop-safety-reviewer`、`postprocess-exporter`、`physics-field-pipeline-auditor` 等专项技能中执行 Python 脚本的硬编码路径，从 `python .gemini/...` 全面更新为 `python .agents/...`。
- [x] **测试脚本与文档**：更新了 `.agents/tests/check_grpd_antigravity_overlay.py` 自测脚本中 `REQUIRED_FILES` 与主函数的路径断言。更新了 `.agents/README.md` 的架构图与文档说明。

---

## 验证结果

### 自动化校验
在 `.agents/` 路径下直接运行校验脚本：
```bash
python .agents/tests/check_grpd_antigravity_overlay.py
```
**结果：** 50 项约束检查全部通过（Passed: 50, Failed: 0），完全符合预期的重构规范。
