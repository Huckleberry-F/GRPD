# 网格、邻域与接触搜索

## 关键文件

- `PDCommon/IO/include/MeshData.h`
- `PDCommon/Model/include/ParticleManager.h`
- `PDCommon/Neighbor/include/NeighborList.h`
- `PDCommon/Neighbor/src/NeighborList.cpp`
- `PDCommon/Contact/include/ContactSpatialGrid.h`
- `PDCommon/Contact/src/ContactSpatialGrid.cpp`
- `Src/Engine/Solvers/PD/Pre/src/InitModel.cpp`
- `Src/Engine/Solvers/PD/Pre/src/InitNeighbors.cpp`
- `Src/Engine/Solvers/PD/Pre/src/InitContact.cpp`

## NeighborList

- 使用 flat cell list + CSR。
- Pass 1 并行计数。
- Prefix sum 生成 `offsets_`。
- Pass 2 并行填充 `neighborIds_` 和 `bondLengths_`。
- 当前按 `PartID` 过滤材料邻域。

## ContactSpatialGrid

- 使用 master ids 建 grid。
- 使用 `head_` / `next_` 链表。
- 考虑 `ActiveStatus`。
- 有 NaN、超大 bounds、超大 cell 数保护。

## 检查点

- 区分材料邻域、几何邻域、接触候选。
- 检查 `horizon/dx` 比例。
- 2D 算例仍存 3D 坐标时检查厚度和体积。
- 粒子失效后确认邻域或接触是否需要重建或跳过。
- 大模型要估算 CSR 和 contact grid 内存。
