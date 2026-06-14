# 材料与状态变量规则

## 关键文件

- `PDCommon/Material/include/Material.h`
- `PDCommon/Material/include/MechanicalMaterial.h`
- `PDCommon/Material/include/ThermalMaterial.h`
- `PDCommon/Material/src/*Mat.cpp`
- `Src/Engine/Solvers/PD/Pre/src/InitMaterial.cpp`
- `Src/Engine/Solvers/PD/Pre/src/InitFields.cpp`

## 生命周期

1. `InitMaterial` 从 YAML 读取材料，使用 `MaterialRegistry` 创建实例。
2. `initialize(const YAML::Node&)` 解析材料参数。
3. `InitFields` 调用每个材料的 `allocateStateVariables(FieldManager&)`。
4. `FieldManager::resizeAll(numParticles)` 后统一分配内存。
5. `bindStateVariables(FieldManager&)` 缓存 `TypedField` 和裸指针。
6. 时间步末 `FieldManager::executeAllRegisteredSwaps()` 交换 Old/Trial。
7. 材料 `commitState()` 同步指针或提交状态。

## 状态变量约定

- 历史相关状态优先使用 `*_Old` 和 `*_Trial`。
- 需要回合末推进的状态必须注册 `registerSwapPair(old, trial)`。
- 高频计算中不要反复 `getFieldAs`，应在绑定阶段缓存指针。
- 如果使用命名状态场，`getNumStateVariables()` 可以返回 0；如果使用统一 `SDV` 池，必须返回维度。

## 本构检查

- 明确小变形或大变形。
- 明确返回应力是 engineering、PK1、PK2 还是 Cauchy。
- J2/JC 类材料检查屈服函数、等效塑性应变、塑性应变张量、背应力、VonMises 输出。
- 参数范围检查应在 `initialize` 中完成。
