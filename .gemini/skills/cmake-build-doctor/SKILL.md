---
name: cmake-build-doctor
description: "GRPD CMake 构建专项入口。用于诊断或维护 C++17、OpenMP、Eigen、yaml-cpp、VTK/IO、对象库 PDCommon/Src/Third、模块 CMakeLists、源文件遗漏、include 路径、Debug/Release 输出目录和 Windows/MinGW/MSVC 构建问题。"
---

# GRPD CMake 构建入口

使用中心工具包：`../grpd-cae-toolkit`。

## 必做流程

1. 读取 `references/cmake-layout.md`。
2. 运行 `python .gemini/skills/grpd-cae-toolkit/scripts/check_cmake_sources.py .`。
3. 如涉及注册宏，运行 `python .gemini/skills/grpd-cae-toolkit/scripts/list_registry_macros.py .`。

## 判断重点

- 新增 `.cpp` 是否加入对应模块 `CMakeLists.txt`。
- 依赖方向是否保持 `PDCommon` 不反向依赖 `Src`。
- Eigen/yaml-cpp/OpenMP 的 PUBLIC/PRIVATE 链接是否合理。
