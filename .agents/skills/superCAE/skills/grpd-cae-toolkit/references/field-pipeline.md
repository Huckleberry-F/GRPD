# 物理场生成链路

## 关键文件

- `PDCommon/Field/include/PhysicsFieldRegistry.h`
- `PDCommon/Field/include/PhysicsFields.h`
- `PDCommon/Field/src/MechanicalFields.cpp`
- `PDCommon/Field/src/ThermalFields.cpp`
- `PDCommon/Field/include/FieldManager.h`
- `Src/Engine/Solvers/PD/Pre/src/InitFields.cpp`

## 链路

`Solver.Type` -> `PhysicsFieldRegistry` -> `PhysicsFields::registerFields` -> `FieldManager` -> `resizeAll` -> `Kernel/BC/Material/Post`

## 当前核心字段

- Mechanical：`Displacement`、`Velocity`、`Acceleration`、`ContactNormal`、`ContactGap`、`VirtualSurfacePos`。
- Thermal：`Temperature`、`TempRate`、`HeatFlux`。
- MeshData 基础字段：`ID`、`PartID`、`MatID`、`Coords`、`Volume` 等。
- 全局状态：`ActiveStatus`。

## 审计规则

- 新增求解类型时，新增 `PhysicsFields` 子类并用 `REGISTER_PHYSICS_FIELDS` 注册。
- 字段名必须与调用侧字符串完全一致。
- 字段维度必须和物理含义一致：标量 1，向量 3，张量 6 或 9。
- 材料状态变量不应放在 `PhysicsFields` 中，除非它是跨材料共享的物理场。
