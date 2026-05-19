# CMake 构建布局

## 顶层

`CMakeLists.txt` 负责：

- C++17 标准。
- 输出目录。
- `add_subdirectory(PDCommon)`。
- `add_subdirectory(Src)`。
- `add_subdirectory(Third)`。

## PDCommon

`PDCommon/CMakeLists.txt` 使用 object library 聚合：

- `Model_Obj`
- `Field_Obj`
- `Neighbor_Obj`
- `IO_Obj`
- `Material_Obj`
- `BC_Obj`
- `Utils_Obj`
- `Core_Obj`
- `Kernel_Obj`
- `Fracture_Obj`
- `Contact_Obj`
- `PostProcessing_Obj`

OpenMP 通过 `find_package(OpenMP REQUIRED)` 和 `OpenMP::OpenMP_CXX` 链接。

## 新增源文件位置

- 材料：`PDCommon/Material/CMakeLists.txt`
- Kernel/Stabilizer：`PDCommon/Kernel/CMakeLists.txt`
- BC：`PDCommon/BC/CMakeLists.txt`
- Contact：`PDCommon/Contact/CMakeLists.txt`
- Field/PhysicsFields：`PDCommon/Field/CMakeLists.txt`
- TimeIntegrator：`Src/Integration/CMakeLists.txt`
- Engine/Solver：`Src/Engine` 对应 CMake 文件。

## 依赖方向

- `PDCommon` 不应反向依赖 `Src`。
- 头文件中暴露 Eigen 类型时，`Eigen3::Eigen` 通常需要 PUBLIC。
- 仅 `.cpp` 内部使用的依赖优先 PRIVATE。
