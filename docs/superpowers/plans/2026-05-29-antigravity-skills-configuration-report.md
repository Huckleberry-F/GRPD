# GRPD 仓库 AI Skills 体系与 Antigravity 适配总结报告 (面向 Codex 协作使用)

本报告详细说明了当前 GRPD 仓库中的 **AI Skills** 系统配置架构，以及为了适配 **Antigravity (Gemini IDE)** 环境而做出的专属优化与规约。该报告旨在供开发者和 **Codex** 联合研究、调试以及进一步优化工作流时参考。

---

## 一、 技能系统配置与物理目录结构

GRPD 仓库的 AI 技能体系由两类同级技能集合组成，全部物理落盘于版本控制中：

1. **业务专属技能集合 (`superCAE`)**：存放于 [`.gemini/skills/superCAE/skills/`](file:///d:/Project_C++/GRPD/.gemini/skills/superCAE/skills/)
   - 职责：包含 GRPD 核心架构分层、物理场 SoA 内存分配、材料本构数学推导、求解主循环安全评估、高性能 OpenMP 并行设计等专项规范。
2. **流程元技能 (Superpowers)**：存放于 [`.gemini/skills/superpowers/skills/`](file:///d:/Project_C++/GRPD/.gemini/skills/superpowers/skills/)
   - 职责：约束 AI 代理的标准工程开发流程（头脑风暴 $\rightarrow$ 编写计划 $\rightarrow$ 逐步执行 $\rightarrow$ 测试验证 $\rightarrow$ 分支合并与评审）。
3. **CAE 路由入口**：存放于 [`.gemini/skills/superCAE/skills/grpd-cae-router/`](file:///d:/Project_C++/GRPD/.gemini/skills/superCAE/skills/grpd-cae-router/)
   - 职责：作为 `superpowers` 与 `superCAE` 之间的连接层，根据材料、求解器、Kernel、YAML、VTK、CMake、数值验证等影响面选择专项 skill。

---

## 二、 核心瓶颈与 Antigravity 适配方案 (Plan C)

### 1. 瓶颈背景
在 **Antigravity (Gemini IDE)** 宿主环境下，AI 代理没有暴露原生的 `activate_skill` 或类似 `Skill` 工具，这意味着 **系统无法自动在后台静默加载工作区自定义技能**。

### 2. 适配解决方案：Plan C 物理读取
为了让 AI 代理能够正确遵循技能规约，我们在 [AGENTS.md](file:///d:/Project_C++/GRPD/AGENTS.md) 中硬编码了**“C方案”物理加载规约**：
- 当接收到相关复杂任务时，AI 代理在思考前置阶段必须使用 `view_file` 工具读取对应技能目录下的 `SKILL.md` 物理文件。
- AI 代理必须在对话输出的首行打印 `[Active Skill: <技能名称>]` 宣告加载，并强制执行对应的 Checklist。
- 全局遵循**中文输出**与**中文注释**的语言锁。

---

## 三、 本次为适配 Antigravity 进行的 Skills 改动

为了防范 Antigravity 原生 IDE 机制与 Superpowers 默认行为发生冲突，我们对以下两个核心技能进行了针对性修改：

### 1. [`using-superpowers/SKILL.md`](file:///d:/Project_C++/GRPD/.gemini/skills/superpowers/skills/using-superpowers/SKILL.md)
* **写入 Antigravity 加载路径**：在 "How to Access Skills" 中写入物理读取规约。
* **屏蔽基础终端指令**：在 "Platform Adaptation" 中强制规定，AI 代理必须优先使用 IDE 内置的高性能原生 API（如 `list_dir`、`view_file`、`grep_search`、`replace_file_content`），**严禁**随意在 bash 终端中通过 `ls`、`cat`、`sed` 等命令行工具修改或检索文件。
* **禁止向系统 Artifact 目录写入**：防止工程文件写入隐藏系统路径。

### 2. [`writing-plans/SKILL.md`](file:///d:/Project_C++/GRPD/.gemini/skills/superpowers/skills/writing-plans/SKILL.md)
* **⚠️ 终极工作流拦截机制**：Antigravity 原生具有极强的“Planning Mode”倾向，总想在后台生成 `brain/<conv-id>/implementation_plan.md` 等原生 Artifacts。为此我们在技能中追加了 **CRITICAL ANTIGRAVITY RULE**：**绝对禁止向系统隐藏 Artifact 路径写入计划**，所有的开发计划（Plans）与总结（Walkthroughs）必须以物理 `.md` 文件的形式，写入项目内可见的 `docs/superpowers/plans/` 目录下（如 `YYYY-MM-DD-<feature-name>.md`）。

---

## 四、 已安装的 Skills 清单

### 1. GRPD 业务专属技能集合 (`superCAE`)
所有 GRPD/CAE 专项技能统一位于 `.gemini/skills/superCAE/skills/`，与 `.gemini/skills/superpowers/` 同级。

* **`grpd-cae-router`**：根据 CAE 影响面选择专项 skill、风险字段和验证路径。
* **`cae-input-schema-validator`**：校验 `PD.yaml` 输入卡的量纲与默认值。
* **`cmake-build-doctor`**：诊断和维护 C++17、OpenMP、Eigen 构建链路。
* **`constitutive-math-reviewer`**：审查本构理论、径向返回算法及微模量数学公式的准确性。
* **`grpd-cae-toolkit`**：数据审计脚手架，分析 SoA 物理场、求解器初始化及输出共享链路。
* **`grpd-experiment-tracker`**：基于 SQLite 比对不同参数/版本下的收敛残差和计算耗时。
* **`grpd-smoke-tester`**：一键式运行 GRPD 求解、ANSYS 求解并输出融合误差报告。
* **`material-model-implementer`**：指导实现材料子类、状态变量分配和提交。
* **`mesh-and-neighbor-reviewer`**：审查网格拓扑、近邻 CSR 列表以及接触搜索的空间算法。
* **`numerical-test-generator`**：生成 Patch Test、单点拉伸等数值验证算例。
* **`openmp-kernel-optimizer`**：优化高频物理循环的 Cache 局部性与 OpenMP 线程安全（防止伪共享）。
* **`physics-field-pipeline-auditor`**：审计物理场生成与接管管线。
* **`postprocess-exporter`**：审计 VTK/PVD 数据写入与 ParaView 后处理的兼容性。
* **`solver-loop-safety-reviewer`**：规范时间积分器在主循环中的初始化、边界施加和状态提交顺序。

### 2. Superpowers 元技能
* **过程指引**：`brainstorming`（头脑风暴）、`systematic-debugging`（系统化缺陷调试）、`test-driven-development`（测试驱动开发）。
* **计划与执行**：`writing-plans`（编写计划）、`executing-plans`（分步执行计划）、`verification-before-completion`（完工自检）、`finishing-a-development-branch`（分支收尾与合并）。
* **多 Agent 调度**：`dispatching-parallel-agents`（多 Agent 派发）、`subagent-driven-development`（子 Agent 驱动开发）。
* **代码评审**：`requesting-code-review`（发起评审）、`receiving-code-review`（消化评审意见）。
* **工具管理**：`using-git-worktrees`（安全利用 Worktree）、`writing-skills`（沉淀并撰写新 Skill）。

---

## 五、 与 MCP (Model Context Protocol) 服务的协同

本地技能与项目 `tools/` 目录下的 MCP 服务器强绑定。AI 代理可以在执行本地技能时调用以下 MCP 工具：
1. **`ansys-mcp-server`**：自动调度本地 ANSYS APDL 执行基准仿真并导出对比数据。
2. **`path-comparison-mcp-server`**：用于自动对齐、空间插值和数值路径切片对比。
3. **`grpd-mcp-server`**：一键编译和运行 GRPD 引擎并监视日志。
4. **`grpd-experiment-mcp-server`**：SQLite 实验管理服务器，追踪残差收敛曲线和物理用时。
5. **`point-integration-matlab-mcp-server`**：调用 MATLAB 引擎运行三维本构计算的单点应力更新比对。

---

## 六、 供您与 Codex 深入研究的课题建议

1. **自动生成 Plan C 宣告脚手架**：
   在宿主环境限制下，能否为 Codex 配置专属的工作流脚本，以便在新对话建立时，自动将当前上下文所需的技能头（`[Active Skill: xxx]`）以前置 System Prompt 的形式注入，免去 AI 代理在每次会话开始时频繁调用 `view_file` 物理检索技能文件的性能开销。
2. **测试与回归闭环（grpd-smoke-tester 与 TDD 结合）**：
   将 `test-driven-development` 技能与本地编译后的测试套件（如 `test_constitutive.exe` 与 `test_math_utils.exe`）进行深度绑定，自动配置 JSON 驱动的路径拉伸算例，实现材料本构的自动化数值检验。
3. **隐式求解器的准静态收敛性分析**：
   既然隐式求解器 `MatrixFreeImplicitIntegrator` 的状态丢失 Bug 已经修复，未来可以针对隐式静力分析，利用 `grpd-experiment-mcp-server` 专门收集不同算法（JFNK 与 L-BFGS）在不同公差（`NRForceTol`）下的残差迭代收敛图线，自动化调整线搜索及扰动步长参数。
