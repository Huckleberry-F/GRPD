# GRPD Project Coding Standards (GEMINI.md)

作为 GRPD 项目的 AI 助手，在阅读、编写或重构代码时，必须严格遵守以下原则：

## 1. 高效 C++ 编程

- 使用 C++17 标准。
- 优先考虑性能，避免不必要的对象拷贝。
- 使用智能指针（如 `std::unique_ptr`）进行资源管理。

## 2. 并行加速 (OpenMP)

- 对计算密集型循环（如粒子遍历、受力计算）使用 `#pragma omp parallel for` 进行加速。
- 确保在 CMake 中正确配置 OpenMP 支持。

## 3. 设计模式与模块化

对于项目中的核心模块（如 `Field`, `Material`, `BC`），应统一遵循以下模式：

- **Manager (管理器)**: 统一持有和管理该模块的所有实例（如 `FieldManager`）。
- **Registry (注册表)**: 提供全局注册中心，支持编译期静态注册。
- **Factory (工厂)**: 通过字符串类型名动态创建具体子类实例。
- **Singleton (单例)**: 注册表类通常实现为单例模式（如 `FieldRegistry::getInstance()`）。

## 4. 模块间解耦

- 模块之间必须保持高度解耦。例如，材料本构模型不应直接依赖于具体的求解器核心。
- 使用基类接口和多态性进行交互。

## 5. 物理场生成 (Physics Type to Field)

- 通过物理问题类型（如 "Thermal", "Mechanical"）生成对应的待求解物理场。
- 使用 `PhysicsFieldRegistry` 和 `PhysicsFields` 子类来自动向 `FieldManager` 注册所需字段。

## 6. 材料状态变量 (Material to State Field)

- 通过材料类型生成对应的内部状态变量场。
- 每个 `Material` 子类应实现 `allocateStateVariables(FieldManager&)` 方法。

## 7. 源码组织 (H/CPP 分离)

- 严禁在 `.h` 文件中实现复杂的函数逻辑。
- `.h` 文件仅存放类定义和函数声明。
- `.cpp` 文件负责具体的函数实现。

## 8. 回答问题

- 永远使用中文回答plan、Task等文件内容，以及对话结论等。
