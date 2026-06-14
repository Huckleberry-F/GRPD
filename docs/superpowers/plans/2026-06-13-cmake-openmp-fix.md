# macOS 环境下 CMake 找不到 OpenMP 的修复计划

> **给 Agent 执行者的提示：** 必须使用 `superpowers:executing-plans` 来逐步执行本计划。步骤使用复选框 (`- [ ]`) 语法进行追踪。

**目标：** 解决在 macOS 上使用 Apple Clang 时，CMake 无法定位和链接 OpenMP 的问题。

**方案架构：** 在 `PDCommon/CMakeLists.txt` 中引入针对 macOS (Apple) 及 AppleClang 编译器的特殊检测。如果发现 Homebrew 安装的 `libomp`，则自动配置 OpenMP 编译及链接参数，并注册导入的虚拟目标 `OpenMP::OpenMP_CXX`，从而绕过官方 `find_package(OpenMP REQUIRED)` 在 Apple Clang 下无法直接支持的报错。

**技术栈：** CMake, Apple Clang, Homebrew libomp

---

### 任务 1：修改 `PDCommon/CMakeLists.txt` 适配 macOS 的 OpenMP 查找

**相关文件：**
- 修改：`file:///Users/hanbozhang/C++/GRPD/PDCommon/CMakeLists.txt`

- [ ] **步骤 1：在 `PDCommon/CMakeLists.txt` 顶部（查找 OpenMP 之前）插入针对 macOS / AppleClang + Homebrew libomp 的特定适配代码**

  需要修改的目标内容片段为：
  ```cmake
  # 1. Find OpenMP package (REQUIRED means error if not found)
  find_package(OpenMP REQUIRED)
  ```
  将其修改替换为：
  ```cmake
  # 1. For macOS (Apple) with AppleClang, try to find Homebrew's libomp to satisfy OpenMP dependency
  if(APPLE AND CMAKE_CXX_COMPILER_ID MATCHES "AppleClang")
      # Search for omp.h in standard Homebrew paths
      find_path(OpenMP_INCLUDES omp.h HINTS "/opt/homebrew/opt/libomp/include" "/usr/local/opt/libomp/include")
      # Search for libomp in standard Homebrew paths
      find_library(OpenMP_LIBRARIES omp HINTS "/opt/homebrew/opt/libomp/lib" "/usr/local/opt/libomp/lib")
      
      if(OpenMP_INCLUDES AND OpenMP_LIBRARIES)
          set(OpenMP_CXX_FLAGS "-Xpreprocessor -fopenmp")
          set(OpenMP_CXX_FOUND TRUE)
          set(OpenMP_FOUND TRUE)
          if(NOT TARGET OpenMP::OpenMP_CXX)
              add_library(OpenMP::OpenMP_CXX IMPORTED INTERFACE)
              set_target_properties(OpenMP::OpenMP_CXX PROPERTIES
                  INTERFACE_COMPILE_OPTIONS "-Xpreprocessor;-fopenmp"
                  INTERFACE_INCLUDE_DIRECTORIES "${OpenMP_INCLUDES}"
                  INTERFACE_LINK_LIBRARIES "${OpenMP_LIBRARIES}"
              )
          endif()
          message(STATUS "macOS AppleClang: Found Homebrew OpenMP at ${OpenMP_INCLUDES}")
      endif()
  endif()

  # Find OpenMP package (if not already found via our custom AppleClang helper)
  if(NOT OpenMP_FOUND)
      find_package(OpenMP REQUIRED)
  endif()
  ```

- [ ] **步骤 2：对 `PDCommon/CMakeLists.txt` 后续的 `OpenMP_CXX_FOUND` 检查逻辑进行微调**
  
  确保 `OpenMP_CXX_FOUND` 被正确处理。我们编写的适配代码中已经手动设置了 `set(OpenMP_CXX_FOUND TRUE)`，后续判断 `if(OpenMP_CXX_FOUND)` 将会自然进入。

### 任务 2：执行 CMake 配置和编译验证

**相关文件：**
- 无新文件。

- [ ] **步骤 1：清理并重新配置 CMake**
  
  在工程根目录下（比如 `/Users/hanbozhang/C++/GRPD`），执行以下命令以验证配置阶段：
  ```bash
  rm -rf build && mkdir build && cd build && cmake ..
  ```
  确认没有出现 `Could NOT find OpenMP_CXX` 的错误。

- [ ] **步骤 2：执行编译**
  
  在 `build` 目录下运行：
  ```bash
  make -j$(sysctl -n hw.ncpu)
  ```
  确认所有目标（特别是 `PDCommon` 静态库以及相关的子模块）均可以成功编译和链接。

- [ ] **步骤 3：提交修改**
  
  运行：
  ```bash
  git add PDCommon/CMakeLists.txt
  git commit -m "build: fix CMake OpenMP detection on macOS with AppleClang"
  ```
