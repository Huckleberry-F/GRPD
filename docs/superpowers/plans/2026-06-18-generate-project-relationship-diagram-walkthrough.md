# GRPD 项目关系图生成 Walkthrough

本文件总结了利用 `codegraph` 检索到的代码信息生成 GRPD 项目架构与核心关系图的过程。

## 变更概述

我们创建了物理文件 [docs/project_relationship.md](file:///Users/hanbozhang/C++/GRPD/docs/project_relationship.md)，在此文件中采用 Mermaid.js 绘制了以下图表：

1. **8层计算管线执行流图**：严格映射 `AGENTS.md` 中的规范（IO -> 拓扑 -> SoA场 -> 积分器/BC -> 接触 -> PD核 -> 本构/状态 -> 损伤断裂），显示了数据和控制流的隔离模式。
2. **核心类继承体系图**：列出了接口与派生子类关系，包括：
   - 物理内核：`PDKernel` 派生出 `BB_Base` 和 `NOSB_Base`，后者进而派生出力学、热学及轴对称具体内核。
   - 本构材料：`Material` -> `MechanicalMaterial` (弹性、J2塑性、隐式损伤塑性、蠕变) / `ThermalMaterial` (傅里叶热传导)。
   - 时间积分器：显式欧拉、中心差分、自适应动态弛豫(ADR)、热-力交替积分。
   - 边界条件：力学 BC (位移、体力、速度) 与热学 BC (温度、热通量、对流)。
   - 损伤断裂：键伸长、等效塑性应变、损伤值断裂模型。
   - 网格解析与接触：`MeshReader`、`IContactAlgorithm` 的层级结构。
3. **PDContext 核心数据中枢关系图**：展示了 `PDContext` 作为数据总线与 `PDEngine` 和各 `Manager` 的组合与驱动关系。
4. **注册表与工厂单例模式**：归纳了项目中 10 个具体的单例注册表及其与 Manager 和 Base 的动态创建模式。
5. **模块间依赖方向图**：分析并展示了 `PDCommon` 下的各子文件夹与 `Src` 核心之间的单向非循环依赖关系。

## 验证与测试

1. **Mermaid 语法合规性检查**：已对 Markdown 文件中的 Mermaid 语法进行排查，验证了其括号对、类图定义、流图方向等格式，确保支持良好的可视化渲染。
2. **代码映射验证**：通过此前收集的 `codegraph_explore` 结果，证实了类名、方法和模块职责与实际 C++ 代码完全吻合。
3. **AGENTS.md 规范符合性**：所有图表及中文注释均严格落实了仓库的语言及核心设计原则。

## 文件改动详情

新建了以下物理文件：
- `docs/project_relationship.md` (项目关系图主文档)
- `docs/superpowers/plans/2026-06-18-generate-project-relationship-diagram-walkthrough.md` (本总结文档)
