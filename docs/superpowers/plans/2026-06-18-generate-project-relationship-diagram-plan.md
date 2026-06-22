# GRPD 项目关系图生成实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 根据 `codegraph` 检索和分析得到的数据，绘制并生成 GRPD C++ 求解器的项目关系图及模块依赖文档，物理落盘到工程中。

**Architecture:** 
文档将采用 Mermaid.js 格式构建，包含以下几个核心视图：
1. **8层计算管线执行流图**：对应 `AGENTS.md` 中的 8 层设计，展示从输入解析到损伤演化的调用链。
2. **核心类继承体系图**：展示 `Engine`, `PDKernel`, `Material`, `TimeIntegrator`, `FractureModel`, `BC`, `MeshReader` 等接口与其具体实现的派生关系。
3. **PDContext 核心数据中枢关系图**：展示 `PDContext` 如何组合管理各个 Manager 与核心场。
4. **编译期注册表与工厂模式图**：展示各类 `Registry` 和 `Factory` 的单例设计。
5. **模块间依赖方向图**：展示 `PDCommon` 下的 12 个子模块及 `Src` 引擎核心层之间的依赖拓扑。

**Tech Stack:** Mermaid.js, Markdown

---

### Task 1: 物理落盘项目关系图主文档

**Files:**
- Create: `docs/project_relationship.md`

- [ ] **Step 1: 编写 docs/project_relationship.md**
  
  在 `docs/project_relationship.md` 中详细绘制 Mermaid 关系图，包含 8 层管线、类继承、PDContext、工厂注册以及模块依赖拓扑，全部使用中文进行注释和说明，符合 `AGENTS.md` 语言要求。

- [ ] **Step 2: 验证 docs/project_relationship.md 的 Mermaid 语法**

  检查 Mermaid 代码块的闭合性、箭头方向、节点标签括号是否正确转义，确保在 Markdown 渲染器中可以正常加载。

- [ ] **Step 3: 提交修改**

  使用 git 提交新建的 `project_relationship.md` 文件。

---

### Task 2: 物理落盘 Walkthrough 总结文件

**Files:**
- Create: `docs/superpowers/plans/2026-06-18-generate-project-relationship-diagram-walkthrough.md`

- [ ] **Step 1: 编写 2026-06-18-generate-project-relationship-diagram-walkthrough.md**

  在 `docs/superpowers/plans/2026-06-18-generate-project-relationship-diagram-walkthrough.md` 中总结本次生成项目关系图的工作，说明绘制的图表类型与核心关系。

- [ ] **Step 2: 提交 Walkthrough 文件**

  使用 git 提交该 Walkthrough 总结文件。
