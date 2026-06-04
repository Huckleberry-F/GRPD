# GRPD 架构速览

## 分层

- `PDCommon/`：基础库，包含 `Field`、`Material`、`BC`、`Kernel`、`Fracture`、`Contact`、`Neighbor`、`Model`、`IO`、`Core`、`Utils`、`PostProcessing`。
- `Src/`：求解器与时间积分调度，包含 `Engine`、`Integration`。
- `Third/`：第三方依赖，当前包含 `yaml-cpp` 等。
- `Examples/`：`PD.yaml` 示例和输入几何。

## 初始化顺序

`Src/Engine/Solvers/PD/PDEngine.cpp` 中 `PDEngine::Initialize` 当前顺序：

1. `InitModel`
2. `InitMaterial`
3. `InitFields`
4. `InitConditions`
5. `InitNeighbors`
6. `InitContact`
7. `InitFractureModels`
8. `InitPreCracks`
9. `InitSolverComponents`
10. `InitPostProcessing`

改动初始化阶段时，必须说明是否影响上面顺序。

## 边界

- 材料层不直接依赖求解器核心。
- 字段创建集中在 `PhysicsFields`、`MeshData::ensureParticleFields`、`Material::allocateStateVariables`。
- 求解器应通过抽象接口、Manager 和 Registry 组合模块。
- 不要把状态变量分配逻辑散落到 Kernel 或 Integrator 中。
