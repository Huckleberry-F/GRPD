# macOS 环境下 CMake 找不到 OpenMP 修复总结

我正在使用 `finishing-a-development-branch` 技能来总结本次开发。

## 已完成的变更

本次修改主要解决了在 macOS 系统下，使用 Apple Clang 编译器无法正常定位和链接 OpenMP，导致 CMake 报错的问题。同时，在修复此构建问题的过程中，一并解决了 `PDCommon/IO/src/IOManager.cpp` 中由于使用了非跨平台的 API（`localtime_s` 与 Linux 专有的 `/proc/self/exe`）导致 macOS 下编译失败的问题，并完成了对 `Third/eigen` 第三方依赖子模块的克隆与拉取。

### 1. [PDCommon/CMakeLists.txt](file:///Users/hanbozhang/C++/GRPD/PDCommon/CMakeLists.txt)
在 `PDCommon/CMakeLists.txt` 顶部增加了针对 macOS (Apple) 及 AppleClang 编译器的特殊处理：
- 显式地在 Homebrew 的标准路径下（如 `/opt/homebrew/opt/libomp` 等）查找 `omp.h` 与 `libomp`。
- 如果找到，则手动声明 OpenMP 构建变量，并注册导入的虚拟目标 `OpenMP::OpenMP_CXX`。
- 将后续的 `find_package(OpenMP REQUIRED)` 包含在条件判断中，避免再次触发原生查找失败的报错。

### 2. [IOManager.cpp](file:///Users/hanbozhang/C++/GRPD/PDCommon/IO/src/IOManager.cpp)
修复了多处 macOS 下的编译报错与运行时潜在崩溃缺陷：
- 引入 `<mach-o/dyld.h>`，并使用 `_NSGetExecutablePath` 来在 macOS 下安全推导当前运行的可执行程序所在根目录，防止非 Windows 平台一律读取 `/proc/self/exe` 导致的运行时崩溃（macOS 无 `/proc`）。
- 将 `localtime_s` 包装为跨平台版本，在 Windows 环境下保持 `localtime_s`，而在非 Windows 环境下使用线程安全的 `localtime_r`。

### 3. [Third/eigen](file:///Users/hanbozhang/C++/GRPD/Third/eigen)
由于原仓库的子模块 Eigen 引用了不可用的 commit 哈希导致 `git submodule update` 报 `fatal: not our ref` 错误，我们将其重新克隆为最新的稳定版 `3.4.0`，从而使得 `Third/CMakeLists.txt` 能够成功找到 `eigen/CMakeLists.txt`，解除了构建阻塞。

---

## 变更 Diff

### [PDCommon/CMakeLists.txt](file:///Users/hanbozhang/C++/GRPD/PDCommon/CMakeLists.txt)
```diff
@@ -4,5 +4,30 @@
-# 1. Find OpenMP package (REQUIRED means error if not found)
-find_package(OpenMP REQUIRED)
+# 1. For macOS (Apple) with AppleClang, try to find Homebrew's libomp to satisfy OpenMP dependency
+if(APPLE AND CMAKE_CXX_COMPILER_ID MATCHES "AppleClang")
+    # Search for omp.h in standard Homebrew paths
+    find_path(OpenMP_INCLUDES omp.h HINTS "/opt/homebrew/opt/libomp/include" "/usr/local/opt/libomp/include")
+    # Search for libomp in standard Homebrew paths
+    find_library(OpenMP_LIBRARIES omp HINTS "/opt/homebrew/opt/libomp/lib" "/usr/local/opt/libomp/lib")
+    
+    if(OpenMP_INCLUDES AND OpenMP_LIBRARIES)
+        set(OpenMP_CXX_FLAGS "-Xpreprocessor -fopenmp")
+        set(OpenMP_CXX_FOUND TRUE)
+        set(OpenMP_FOUND TRUE)
+        if(NOT TARGET OpenMP::OpenMP_CXX)
+            add_library(OpenMP::OpenMP_CXX IMPORTED INTERFACE)
+            set_target_properties(OpenMP::OpenMP_CXX PROPERTIES
+                INTERFACE_COMPILE_OPTIONS "-Xpreprocessor;-fopenmp"
+                INTERFACE_INCLUDE_DIRECTORIES "${OpenMP_INCLUDES}"
+                INTERFACE_LINK_LIBRARIES "${OpenMP_LIBRARIES}"
+            )
+        endif()
+        message(STATUS "macOS AppleClang: Found Homebrew OpenMP at ${OpenMP_INCLUDES}")
+    endif()
+endif()
+
+# Find OpenMP package (if not already found via our custom AppleClang helper)
+if(NOT OpenMP_FOUND)
+    find_package(OpenMP REQUIRED)
+endif()
```

### [PDCommon/IO/src/IOManager.cpp](file:///Users/hanbozhang/C++/GRPD/PDCommon/IO/src/IOManager.cpp)
```diff
@@ -11,6 +11,8 @@
 
 #ifdef _WIN32
 #include <windows.h>
+#elif defined(__APPLE__)
+#include <mach-o/dyld.h>
 #endif
 
 namespace PDCommon::IO {
@@ -50,10 +52,20 @@
   wchar_t exeBuf[MAX_PATH];
   GetModuleFileNameW(nullptr, exeBuf, MAX_PATH);
   fs::path exePath(exeBuf);
+  installDir_ = exePath.parent_path().parent_path().parent_path();
+#elif defined(__APPLE__)
+  char path[1024];
+  uint32_t size = sizeof(path);
+  if (_NSGetExecutablePath(path, &size) == 0) {
+    fs::path exePath(path);
+    installDir_ = exePath.parent_path().parent_path().parent_path();
+  } else {
+    installDir_ = fs::current_path();
+  }
 #else
   fs::path exePath = fs::read_symlink("/proc/self/exe");
-#endif
-  installDir_ = exePath.parent_path().parent_path().parent_path();
+  installDir_ = exePath.parent_path().parent_path().parent_path();
+#endif
   LOG_INFO("[IOManager] Install directory: " + installDir_.string());
 
   // ===========================================================
@@ -94,8 +96,11 @@
   std::time_t now_time = std::chrono::system_clock::to_time_t(now);
   std::tm local_tm{};
 
-  // Windows 安全版本的 localtime
+#ifdef _WIN32
   localtime_s(&local_tm, &now_time);
+#else
+  localtime_r(&now_time, &local_tm);
+#endif
 
   char timeBuffer[64];
   std::snprintf(timeBuffer, sizeof(timeBuffer), "Result_%04d%02d%02d_%02d%02d%02d",
```

---

## 验证与测试结果

1. **CMake 配置与生成成功**
   ```bash
   rm -rf build && mkdir build && cd build && cmake ..
   ```
   输出成功完成配置与生成，无 OpenMP 或 Eigen 的缺失报错。

2. **核心工程编译通过**
   ```bash
   make -j$(sysctl -n hw.ncpu)
   ```
   100% 成功生成 `GRPD` 核心解算器、`libPDCommon.a` 静态库以及相关的单元测试程序，未出现编译警告之外的任何构建阻碍。

3. **单元测试验证**
   执行 `./bin/release/test_math_utils`：
   ```
   [test_math_utils] Starting TDD test...
   [test_math_utils] All tests PASSED!
   ```
   测试全部顺利通过。
