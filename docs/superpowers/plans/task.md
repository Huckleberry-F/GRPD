# 仓库清理与文档归类任务进度 (task.md)

- [x] **Task 1: 创建归类所需的新文件夹**
  - [x] 步骤 1：物理创建目标目录 `docs/references` 和 `docs/superpowers/plans/archive`
- [x] **Task 2: 归类整理保留文件**
  - [x] 步骤 1：移动文献 PDF `osti_2566435.pdf` 到 `docs/references/`
  - [x] 步骤 2：移动 YAML 字段速查字典 `GRPD_YAML_Dictionary.md` 到 `docs/`
  - [x] 步骤 3：移动架构说明 `架构说明_PPT.md` 到 `docs/`
  - [x] 步骤 4：将历史开发计划 `Contact_V5_Architecture_Plan.md` 和 `Fracture_IMEX_Plan.md` 移动到 `docs/superpowers/plans/archive/`
- [x] **Task 3: 彻底清理根目录下无用的脚本、冗余大文件与日志**
  - [x] 步骤 1：使用 git rm 彻底清理并移除版本控制的脚本与文件
  - [x] 步骤 2：物理删除未跟踪的临时日志与备份文件
- [x] **Task 4: 清理 Doxygen 网页输出并更新忽略规则**
  - [x] 步骤 1：从 Git 仓库中物理移除已跟踪 of Doxygen 生成文件目录 `docs/html` 和 `docs/latex`
  - [x] 步骤 2：在 `.gitignore` 中追加对 Doxygen 自动生成路径的屏蔽
- [x] **Task 5: 提交更改并验证状态**
  - [x] 步骤 1：验证 git 状态，确认没有漏网或冲突文件
  - [x] 步骤 2：提交清理与重构 Commit
