# Registry / Factory / Manager 模式

## 统一模式

GRPD 中核心模块通常遵循：

- `Manager`：持有实例并提供查询。
- `Registry`：全局注册中心，通常是 `getInstance()` 单例。
- `Factory`：通过字符串类型名创建具体对象。
- 静态注册宏：在 `.cpp` 中把具体类注册到对应 registry。

## 常见宏

- `REGISTER_MATERIAL_TYPE(Type, Creator)`
- `REGISTER_BC_TYPE(Type, Creator)`
- `REGISTER_PHYSICS_FIELDS(Type, Class)`
- `REGISTER_KERNEL(TypeName, Class)`
- `REGISTER_STABILIZER(TypeName, ClassName)`
- `REGISTER_TIME_INTEGRATOR(TypeName, Class)`
- `REGISTER_ENGINE_TYPE(Type, Creator)`

## 检查点

- 新增类是否有 `.h` 和 `.cpp`。
- 注册宏是否放在 `.cpp`。
- 注册名是否与 YAML 中 `Type`、`Kernel`、`TimeIntegrator` 等字符串一致。
- 新增 `.cpp` 是否加入对应模块 `CMakeLists.txt`。
- `Manager` 是否负责持有对象，而不是让调用方裸 owning pointer 管理生命周期。
