# YAML 输入与后处理输出

## 关键文件

- `GRPD_YAML_Dictionary.md`
- `Examples/*/PD.yaml`
- `Src/Engine/Solvers/PD/Pre/src/*.cpp`
- `Src/Engine/Solvers/PD/Post/src/PDEnginePost.cpp`
- `PDCommon/IO/include/Outputer.h`
- `PDCommon/IO/src/Outputer.cpp`
- `PDCommon/IO/include/VtkWriter.h`
- `PDCommon/IO/src/VtkWriter.cpp`

## YAML 检查

- `Solver.Engine` 通常为 `PD`。
- `Solver.Type` 决定物理字段匹配。
- `Materials` 或 `Material` 中 `Type` 必须匹配 `MaterialRegistry` 注册名。
- `Kernel` / `Kernels` 必须匹配 `KernelRegistry` 注册名。
- `TimeIntegrator` 必须匹配 `TimeIntegratorRegistry` 注册名。
- `BoundaryConditions.Type` 必须匹配 `BCRegistry` 注册名。

## Writer.Variables

- `Coords` 是点坐标，不应重复作为普通变量输出。
- `ID`、`PartID`、`MatID`、`IsSurface`、`ActiveStatus` 当前按 int 字段处理。
- 维度为 1 输出 scalar，维度为 3 输出 vector，维度为 6 或 9 输出 tensor。
- 字段名必须与 `FieldManager` 中字段名完全一致。

## 规则

- 真实读取逻辑以 C++ 代码为准，文档次之。
- 修改输入字段时同步更新 `GRPD_YAML_Dictionary.md` 和示例。
- 对必填字段给出明确错误，不要静默使用危险默认值。
