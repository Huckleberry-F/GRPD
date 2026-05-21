# GRPD Agent 开发者指南：AI Skills 与 MCP 架构说明

本文件详细阐述了 GRPD 项目中为 AI 助手（如 Antigravity）量身定制的 **AI Skills（专属技能）** 与 **MCP（Model Context Protocol，模型上下文协议）服务器** 的逻辑架构及配置规范，以便开发者和 AI 助手能在此项目上高效地协同工作。

---

## 一、 整体设计思想

为了让 AI 助手具备高水准的 CAE（计算机辅助工程）与 PD（近场动力学）开发能力，本项目采用了 **双轨驱动架构**：
1. **AI Skills 技能体系 (`.gemini/skills/`)**：充当 AI 助手的“专业知识库与流程指引”，包含本构数学推导、求解循环安全、物理场链路审计、OpenMP 优化、网格邻域性能等专项领域规范，使得 AI 在分析或修改代码时能遵循统一的项目标准。
2. **MCP 工具服务器 (`tools/`)**：充当 AI 助手的“外部行动手臂”，通过连接本地环境（如驱动 Python 脚本、ANSYS 有限元对比、SQLite 实验库、MATLAB 单点积分比对等），让 AI 助手能够直接进行自动编译、冒烟测试、误差分析与实验参数追踪。

```mermaid
graph TD
    A[Gemini IDE / AI Agent] -->|加载规范| B(AI Skills 体系)
    A -->|调用工具| C[MCP Servers]
    
    subgraph AI Skills 体系 (.gemini/skills/)
        B1[物理场/本构链路审计]
        B2[网格邻域与OpenMP优化]
        B3[求解主循环安全评估]
    end
    
    subgraph MCP Servers (tools/)
        C1[ansys-server] -->|调用| D[ANSYS APDL / 提取误差]
        C2[grpd-server] -->|编译运行| E[GRPD 求解器]
        C3[grpd-experiment-server] -->|数据记录| F[(SQLite 算例实验库)]
        C4[point-integration-matlab-server] -->|单点比对| G[MATLAB 验证端]
        C5[path-comparison-mcp-server] -->|切片对比| H[数据融合输出]
    end
```

---

## 二、 AI Skills 技能体系说明

Skills 存放于项目根目录下的 `.gemini/skills/` 目录中。每一个子目录都包含一个核心的 `SKILL.md` 指引，以及配套的 `references/` 规范文档或 `scripts/` 工具脚本：

| 技能名称 | 对应路径 | 核心职责说明 |
| :--- | :--- | :--- |
| **cae-input-schema-validator** | `skills/cae-input-schema-validator` | 用于检验、自动补全、格式化 YAML（如 `PD.yaml`）输入卡，确保量纲与默认值符合引擎要求。 |
| **cmake-build-doctor** | `skills/cmake-build-doctor` | 诊断和维护 C++17、OpenMP、Eigen 等构建链路，防止 CMake 构建脚本缺失源文件。 |
| **constitutive-math-reviewer** | `skills/constitutive-math-reviewer` | 专注于本构更新算法（如 Voigt/张量转换、J2/JC 塑性、Lemaitre 损伤等返还映射）的数学推导与数值稳定审查。 |
| **grpd-cae-toolkit** | `skills/grpd-cae-toolkit` | 通用 CAE 开发脚手架，包括各种物理场、状态变量分配、求解器初始化及输出共享链路的数据审计脚本。 |
| **grpd-experiment-tracker** | `skills/grpd-experiment-tracker` | 对回归测试、收敛性分析进行历史比对和实验追踪，基于 SQLite 管理不同版本或参数下的求解结果。 |
| **grpd-smoke-tester** | `skills/grpd-smoke-tester` | 一键式全流程冒烟测试验证技能。调用 GRPD 求解、ANSYS 经典求解并输出融合误差报告。 |
| **material-model-implementer** | `skills/material-model-implementer` | 指引如何实现和向工厂注册新的材料子类（如 Mechanical / Thermal Material），以及分配与提交历史状态变量。 |
| **mesh-and-neighbor-reviewer** | `skills/mesh-and-neighbor-reviewer` | 粒子网格拓扑、近邻 CSR 列表以及高频接触搜索的空间网格算法合理性与性能审查。 |
| **numerical-test-generator** | `skills/numerical-test-generator` | 设计 patch test、单点积分拉伸等高可靠性的数值收敛与误差回归测试算例。 |
| **openmp-kernel-optimizer** | `skills/openmp-kernel-optimizer` | 评估热循环代码，确保连续场裸指针直接访问的高速缓存局部性与 OpenMP 线程安全（防 False Sharing）。 |
| **physics-field-pipeline-auditor**| `skills/physics-field-pipeline-auditor`| 检查物理场注册链路（Physics Type -> Fields），保障字段被正确分配并接管。 |
| **postprocess-exporter** | `skills/postprocess-exporter` | 检查后处理 VTK / PVD 数据写入的字段类型（标量/向量/张量）与 ParaView 后处理的命名对应规范。 |
| **solver-loop-safety-reviewer** | `skills/solver-loop-safety-reviewer` | 规范时间积分器（如 ADR 弛豫步或无矩阵隐式迭代）在主循环中的初始化、边界施加与状态提交顺序。 |

---

## 三、 MCP (Model Context Protocol) 服务逻辑说明

MCP 服务端源码位于项目根目录的 `tools/` 目录中，各组件功能如下：

1. **`ansys-mcp-server` (`tools/ansys-mcp-server`)**
   - **功能**：自动生成 APDL 宏命令，调度本地 ANSYS 经典版后台执行基准解计算；并具备 VTK/VTU 文件解析能力，提取基准路径上的位移与应力场。
2. **`path-comparison-mcp-server` (`tools/path-comparison-mcp-server`)**
   - **功能**：对两组后处理数据进行几何对齐、空间插值和数值路径切片对比，输出误差报表与对比图像（Matplotlib 绘图）。
3. **`grpd-mcp-server` (`tools/grpd-mcp-server`)**
   - **功能**：为 AI 提供一键编译及运行 GRPD 引擎的工具，监视终端输出与收敛日志。
4. **`grpd-experiment-mcp-server` (`tools/grpd-experiment-mcp-server`)**
   - **功能**：提供 SQLite 数据存储，自动解析物理场数据与日志，持久化记录各算例的残差收敛曲线、网格节点数、物理用时和内存占用。
5. **`point-integration-matlab-mcp-server` (`tools/point-integration-matlab-mcp-server`)**
   - **功能**：通过 MATLAB 引擎运行材料点单点应力更新与积分器安全验证，便于快速比对三维本构计算的数值精确度。

---

## 四、 协作开发与配置规范

为了让团队的所有开发者顺畅协作，项目建立了以下 Git 管理规则：

1. **绝对路径隔离**：
   - 所有的本地运行配置都会保存在 `.gemini/settings.json` 中。
   - 该文件**被 Git 强行忽略**（在 `.gitignore` 中配置），防止包含特定开发者磁盘绝对路径的配置被上传，进而导致其他环境无法使用或频繁发生冲突。
2. **模板配置**：
   - 远端仓库跟踪 [.gemini/settings.json.example](file:///d:/Project_C++/GRPD/.gemini/settings.json.example) 作为配置模板。
   - 新环境拉取代码后，应将 `settings.json.example` 复制为本地非跟踪的 `settings.json`，并将其中的 `<GRPD_PROJECT_ROOT_PATH>` 整体替换为本地的项目根路径即可直接调用。
3. **AI 技能迭代**：
   - 在日常开发中，如果重构了算法或定义了新的注册规范，应在 `.gemini/skills/` 的相关参考（`references/`）中更新开发原则（如在本构审查中添加新的屈服函数积分标准）。
   - 修改后直接执行 Git 提交即可。因为 `.gemini/skills` 已经被纳入了版本控制，下一次 AI 助手载入此项目时就会自动遵循最新的设计和性能规约进行代码生成与修改。
