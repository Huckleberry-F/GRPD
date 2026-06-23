# Walkthrough - Python 前处理器自适应内置虚拟环境优化总结

本文件记录了为解决在 Windows 平台（以及其他平台）因未激活虚拟环境运行 GRPD 导致前处理器报错 `ModuleNotFoundError: No module named 'numpy'` 问题的修复过程与测试结果。

## 变更背景与问题

1. **NumPy 缺失报错**：
   在 Windows 下直接运行编译好的 GRPD 可执行文件时，如果启动它的终端环境没有提前运行 `.\.venv\Scripts\Activate.ps1`，则在模型初始化阶段会抛出以下错误：
   ```text
   Traceback (most recent call last):
     File "C:\C++\GRPD\Generate_py\generate_model.py", line 6, in <module>
       import numpy as np
   ModuleNotFoundError: No module named 'numpy'
   [ERROR] [InitModel] Python pre-processor failed! Please check if python is installed and paths are correct.
   ```
2. **原因定位**：
   在 [InitModel.cpp](file:///c:/C++/GRPD/Src/Engine/Solvers/PD/Pre/src/InitModel.cpp) 中，原有的候选解释器列表 `candidates` 对 Windows 平台仅提供了 `"python3"` 和 `"python"` 这两个裸命令，并且检测逻辑在判断路径是否可用时：
   - 只对以 `/` 开头的绝对路径（如 macOS 的 Homebrew 路径）使用了 `std::filesystem::exists` 判定。
   - 对不以 `/` 开头的相对路径则直接当成裸命令并通过 `std::system("... --version")` 来探测。
   因此，当 Windows 系统在未激活虚拟环境的环境中运行时，引擎极易找到不包含 NumPy 依赖库的全局 Python。

## 解决方案与修改内容

### 1. 修改 Python 解释器搜索逻辑

我们在 [InitModel.cpp](file:///c:/C++/GRPD/Src/Engine/Solvers/PD/Pre/src/InitModel.cpp) 中进行了如下修改：
- **增加虚拟环境候选路径**：将项目内的虚拟环境相对路径（考虑到了不同的执行目录层次，如 `.venv`、`../.venv` 和 `../../.venv`）加入探测列表的最前列，并区分 Windows（`Scripts\python.exe`）和非 Windows（`bin/python`）平台。
- **重构路径有效性判定**：不仅针对绝对路径，凡是包含目录分隔符（如 `/` 或 `\`）的相对路径同样使用 `std::filesystem::exists` 进行存在性检测。如果本地虚拟环境存在，则直接命中，避免了将它们误作为裸命令用 `std::system` 探测。

修改细节如下：
```diff
    std::string pythonExe = "python";
-   const std::string candidates[] = {"python3",
-#ifndef _WIN32
-                                     "/opt/homebrew/bin/python3.12",
-                                     "/opt/homebrew/bin/python3",
-                                     "/usr/local/bin/python3",
-#endif
-                                     "python"};
-
-   for (const auto &path : candidates) {
-     if (path[0] == '/') { // 绝对路径优先使用 std::filesystem::exists 检测
-       if (std::filesystem::exists(path)) {
-         pythonExe = path;
-         break;
-       }
-     } else { // 裸命令，通过 std::system 进行版本测试探测
+   const std::string candidates[] = {
+#ifdef _WIN32
+                                     ".venv\\Scripts\\python.exe",
+                                     "..\\.venv\\Scripts\\python.exe",
+                                     "..\\..\\.venv\\Scripts\\python.exe",
+#else
+                                     ".venv/bin/python",
+                                     "../.venv/bin/python",
+                                     "../../.venv/bin/python",
+#endif
+                                     "python3",
+#ifndef _WIN32
+                                     "/opt/homebrew/bin/python3.12",
+                                     "/opt/homebrew/bin/python3",
+                                     "/usr/local/bin/python3",
+#endif
+                                     "python"};
+
+   for (const auto &path : candidates) {
+     // 只要包含路径分隔符（相对或绝对路径），就直接检测文件物理存在性
+     if (path[0] == '/' || path.find('\\') != std::string::npos || path.find('/') != std::string::npos) {
+       if (std::filesystem::exists(path)) {
+         pythonExe = path;
+         break;
+       }
+     } else { // 裸命令，通过 std::system 进行版本测试探测
```

## 验证与测试结果

1. **编译验证**：
   使用 MCP 工具 `build_grpd` 执行了重新编译，引擎与辅助工具均在 Windows (MinGW) 环境下编译通过，生成了新的 `C:\C++\GRPD\bin\release\GRPD.exe`。
2. **算例验证**：
   使用 MCP 工具 `run_grpd_case` 运行了 `Examples/Axisymmetric_Ring` 算例。由于测试是通过 MCP 服务的后台进程隔离启动的（此环境下系统 PATH 变量中并不包含已激活的虚拟环境），修改前的 GRPD 运行该算例会报错 `ModuleNotFoundError`。而在应用修改并重新编译后：
   - 引擎启动时顺利探测到了位于项目根目录的本地 `..\\.venv\\Scripts\\python.exe` 虚拟环境。
   - 成功执行了 `generate_model.py`，未报任何 `numpy` 依赖缺失的异常。
   - 前处理器成功将生成的 `.grpd` 网格载入计算循环，并且 ADR 求解器顺利收敛（`success: true`, `returncode: 0`）。
