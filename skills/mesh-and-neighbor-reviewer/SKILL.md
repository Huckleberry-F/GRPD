---
name: mesh-and-neighbor-reviewer
description: "GRPD 网格邻域专项入口。用于评审粒子/网格数据、MeshData、ParticleManager、NeighborList、CSR 邻域、cell list、ContactSpatialGrid、接触搜索、horizon/dx、part/mat id 映射、ActiveStatus、边界粒子和邻域性能/正确性。"
---

# GRPD 网格邻域入口

使用中心工具包：`../grpd-cae-toolkit`。

## 必做流程

1. 读取 `../grpd-cae-toolkit/references/mesh-neighbor-contact.md`。
2. 读取 `../grpd-cae-toolkit/references/performance-openmp.md`。
3. 如涉及字段，运行 `python .agent/skills/grpd-cae-toolkit/scripts/check_field_references.py .`。

## 判断重点

- 区分材料邻域、几何邻域和接触候选搜索。
- 检查 `horizon/dx`、PartID 过滤、ActiveStatus、CSR 内存和 ContactSpatialGrid bounds。
- 改邻域策略时必须说明对 Kernel、Fracture、Contact 的影响。
