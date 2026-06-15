# VTK 时间后缀序列号修改实施计划

> **给代理执行者：** 必须使用 `superpowers:executing-plans` 按任务逐项执行本计划。步骤使用复选框（`- [ ]`）语法跟踪。

**目标：** 将 GRPD 生成的 VTK 序列文件名从 `模型名_step00001_t0.1000.vtk` 修改为只使用时间变量 `t` 的后缀形式：`模型名_t0.1000.vtk`。

**架构：** 保持 `PDEngine::Output(int step, double physicalTime)` 与 `Post::ExportVTK(..., int step, double physicalTime)` 的既有调用链不变，避免触碰积分器输出回调和求解循环。仅在 `IOManager::buildVtkPath` 中收敛文件名规则，使所有 VTK 输出统一走“单一变量 time”后缀。

**技术栈：** C++17、`std::filesystem`、CMake、现有 `PDCommon::IO::IOManager`、现有 `Src` 测试目标模式。

---

## CAE 路由字段

**触及管线层级：** 层 1 输入/路径管理、层 4 时间积分输出回调的边界接口、后处理 Writer/VTK 输出路径管理。

**领域技能：** `using-superpowers`、`grpd-cae-router`、`postprocess-exporter`、`writing-plans`。

**状态变量风险：** 不涉及材料状态变量、Old/Trial/Commit、`stateMode` 或损伤提交。

**SoA 风险：** 不新增、不读取、不修改 `FieldManager` SoA 字段；只修改输出文件名。

**并行风险：** 不涉及 OpenMP 或核心循环。

**验证：** 运行字段引用检查、Writer 变量检查、单元测试目标构建与执行；可选跑一个最小示例确认 Result 目录下 VTK 文件名。

## 当前证据

- `Src/Engine/Solvers/PD/PDEngine.cpp:112` 当前通过 `outputCallback(step, time)` 调用 `PDEngine::Output`。
- `Src/Engine/Solvers/PD/PDEngine.cpp:127-129` 当前 `PDEngine::Output` 调用 `Post::ExportVTK(pdContext_, yamlPath_, step, physicalTime)`。
- `Src/Engine/Solvers/PD/Post/src/PDEnginePost.cpp:78-79` 当前在 `step >= 0` 时调用 `ioManager.buildVtkPath(finalOutputName, step, physicalTime)`。
- `PDCommon/IO/include/IOManager.h:76-82` 当前声明 `buildVtkPath(const std::string &baseName, int step, double time)`。
- `PDCommon/IO/src/IOManager.cpp:179-185` 当前文件名格式为 `%s_step%05d_t%.4f.vtk`。
- 技能要求检查已执行：
  - `python .agents/skills/superCAE/skills/grpd-cae-toolkit/scripts/check_field_references.py .`
  - 输出：`[OK] 42 referenced field names are covered by detected registrations or known runtime fields.`
  - `python .agents/skills/superCAE/skills/grpd-cae-toolkit/scripts/inspect_writer_variables.py . --yaml Examples/Thermal_Validation_5x10/PD.yaml`
  - 输出：示例 Writer 变量均为 `[OK]`。

## 文件结构

- 修改： `PDCommon/IO/include/IOManager.h`
  - 更新 `buildVtkPath` 注释，说明 `step` 参数仅保留为兼容占位，文件名只按物理时间 `t` 变化。
- 修改： `PDCommon/IO/src/IOManager.cpp`
  - 将 VTK 文件名格式从 `baseName_step%05d_t%.4f.vtk` 改为 `baseName_t%.4f.vtk`。
  - 在实现中显式 `(void)step;`，避免编译器未使用参数警告。
- 新建： `Src/test_io_manager_vtk_path.cpp`
  - 新增一个轻量测试，创建临时工作目录与 `PD.yaml`，初始化 `IOManager`，断言生成文件名不含 `_step` 且等于 `Beam_t0.1250.vtk`。
- 修改： `Src/CMakeLists.txt`
  - 排除主程序源文件扫描中的 `test_io_manager_vtk_path.cpp`。
  - 增加 `test_io_manager_vtk_path` 可执行测试目标，沿用现有测试目标链接方式。

---

### 任务 1： 写出失败测试

**文件：**
- 新建： `Src/test_io_manager_vtk_path.cpp`
- 修改： `Src/CMakeLists.txt`

- [ ] **步骤 1： 新建测试源文件**

在 `Src/test_io_manager_vtk_path.cpp` 写入以下完整内容：

```cpp
#include "IOManager.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

int main() {
  const fs::path oldCwd = fs::current_path();
  const fs::path testDir =
      fs::temp_directory_path() / "grpd_io_manager_vtk_path_test";

  fs::remove_all(testDir);
  fs::create_directories(testDir);

  {
    std::ofstream yaml(testDir / "PD.yaml");
    yaml << "Solver:\n";
    yaml << "  Engine: PD\n";
  }

  fs::current_path(testDir);

  auto &ioManager = PDCommon::IO::IOManager::getInstance();
  ioManager.initialize();

  const std::string vtkPath = ioManager.buildVtkPath("Beam", 42, 0.125);
  const std::string filename = fs::path(vtkPath).filename().string();

  fs::current_path(oldCwd);

  if (filename != "Beam_t0.1250.vtk") {
    std::cerr << "Unexpected VTK filename: " << filename << std::endl;
    return EXIT_FAILURE;
  }

  if (filename.find("_step") != std::string::npos) {
    std::cerr << "VTK filename still contains step suffix: " << filename
              << std::endl;
    return EXIT_FAILURE;
  }

  std::cout << "[OK] VTK filename uses time-only suffix: " << filename
            << std::endl;
  return EXIT_SUCCESS;
}
```

- [ ] **步骤 2： 修改 `Src/CMakeLists.txt`，让测试目标可构建**

在已有 `list(REMOVE_ITEM DIR_SRCS ...)` 附近追加：

```cmake
list(REMOVE_ITEM DIR_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/test_io_manager_vtk_path.cpp")
```

在文件末尾追加测试目标：

```cmake
# 6. Add test_io_manager_vtk_path target
add_executable(test_io_manager_vtk_path test_io_manager_vtk_path.cpp)
target_include_directories(test_io_manager_vtk_path PRIVATE
    ${PROJECT_SOURCE_DIR}/PDCommon/Utils/include
    ${PROJECT_SOURCE_DIR}/PDCommon/IO/include
    ${PROJECT_SOURCE_DIR}/PDCommon/Material/include
    ${PROJECT_SOURCE_DIR}/PDCommon/BC/include
    ${PROJECT_SOURCE_DIR}/PDCommon/Core/include
    ${PROJECT_SOURCE_DIR}/Src
    ${PROJECT_SOURCE_DIR}/Src/Engine/include
)
target_link_libraries(test_io_manager_vtk_path PRIVATE
    PDCommon
    Engine_Obj
    PDSolve_Obj
    Material_Obj
    BC_Obj
    IO_Obj
    Model_Obj
    Neighbor_Obj
    Core_Obj
    Field_Obj
    Kernel_Obj
    Integration_Obj
    Fracture_Obj
    Contact_Obj
    PostProcessing_Obj
)
```

- [ ] **步骤 3： 构建并确认测试失败**

运行：

```bash
cmake --build build --target test_io_manager_vtk_path -j 8
./bin/release/test_io_manager_vtk_path
```

实现前预期：

```text
Unexpected VTK filename: Beam_step00042_t0.1250.vtk
```

如果本地构建目录不是 `build`，先执行：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
```

---

### 任务 2： 修改 VTK 文件名规则

**文件：**
- 修改： `PDCommon/IO/include/IOManager.h`
- 修改： `PDCommon/IO/src/IOManager.cpp`

- [ ] **步骤 1： 更新头文件注释，明确 `step` 为兼容参数**

将 `PDCommon/IO/include/IOManager.h:76-82` 附近的注释替换为：

```cpp
  /// @brief 根据物理时间生成 VTK 文件的完整路径
  /// @param baseName 文件基础名（通常为模型名）
  /// @param step 当前保留为兼容参数；文件名不再使用步号序列
  /// @param time 物理时间，作为 VTK 序列文件唯一变化后缀
  /// @return 完整的 VTK 文件路径字符串
  std::string buildVtkPath(const std::string &baseName, int step,
                           double time) const;
```

- [ ] **步骤 2： 更新实现，只使用 `t` 后缀**

将 `PDCommon/IO/src/IOManager.cpp:179-185` 附近实现替换为：

```cpp
std::string IOManager::buildVtkPath(const std::string &baseName, int step,
                                    double time) const {
  // 生成格式：Result_时间戳/baseName_t0.0100.vtk。
  // step 参数保留在接口中，用于兼容 PDEnginePost 与各积分器输出回调。
  (void)step;

  char buffer[256];
  std::snprintf(buffer, sizeof(buffer), "%s_t%.4f.vtk", baseName.c_str(),
                time);
  return (resultDir_ / std::string(buffer)).string();
}
```

- [ ] **步骤 3： 运行新增测试并确认通过**

运行：

```bash
cmake --build build --target test_io_manager_vtk_path -j 8
./bin/release/test_io_manager_vtk_path
```

预期：

```text
[OK] VTK filename uses time-only suffix: Beam_t0.1250.vtk
```

---

### 任务 3： 后处理链路验证

**文件：**
- 无代码修改。

- [ ] **步骤 1： 运行字段引用检查**

运行：

```bash
python .agents/skills/superCAE/skills/grpd-cae-toolkit/scripts/check_field_references.py .
```

预期：

```text
[OK] 42 referenced field names are covered by detected registrations or known runtime fields.
```

- [ ] **步骤 2： 检查示例 Writer 变量**

运行：

```bash
python .agents/skills/superCAE/skills/grpd-cae-toolkit/scripts/inspect_writer_variables.py . --yaml Examples/Thermal_Validation_5x10/PD.yaml
```

预期：

```text
[OK] Examples/Thermal_Validation_5x10/PD.yaml: Writer variable 'PartID' dimension=1
[OK] Examples/Thermal_Validation_5x10/PD.yaml: Writer variable 'Volume' dimension=1
[OK] Examples/Thermal_Validation_5x10/PD.yaml: Writer variable 'Temperature' dimension=1
[OK] Examples/Thermal_Validation_5x10/PD.yaml: Writer variable 'ID' dimension=1
[OK] Examples/Thermal_Validation_5x10/PD.yaml: Writer variable 'HeatFlux' dimension=1
```

- [ ] **步骤 3： 构建主程序，确认接口兼容**

运行：

```bash
cmake --build build --target GRPD -j 8
```

预期：

```text
[100%] Built target GRPD
```

- [ ] **步骤 4： 可选最小算例输出检查**

如果 anti 环境允许直接执行 GRPD 示例，则进入一个已有示例目录运行主程序；否则跳过本步骤并说明“受 MCP/执行环境限制，未跑完整算例”。

运行：

```bash
cd Examples/Thermal_Validation_5x10
../../bin/release/GRPD
find . -path './Result_*/*.vtk' -maxdepth 2 -type f | sort | tail -n 5
```

预期文件名模式：

```text
./Result_YYYYMMDD_HHMMSS/<WriterName>_t0.0000.vtk
./Result_YYYYMMDD_HHMMSS/<WriterName>_t*.vtk
```

禁止出现的文件名模式：

```text
不得出现 *_step00001_t*.vtk
```

---

## 最终提交建议

- [ ] **步骤 1： 查看 diff**

运行：

```bash
git diff -- PDCommon/IO/include/IOManager.h PDCommon/IO/src/IOManager.cpp Src/test_io_manager_vtk_path.cpp Src/CMakeLists.txt
```

- [ ] **步骤 2： 提交**

运行：

```bash
git add PDCommon/IO/include/IOManager.h PDCommon/IO/src/IOManager.cpp Src/test_io_manager_vtk_path.cpp Src/CMakeLists.txt
git commit -m "fix: use time-only suffix for vtk output files"
```

## 自检结果

- 规格覆盖： 覆盖“修改 vtk 生成的序列号”“单一变量”“按照时间 t 变化的结尾”。
- 占位符扫描： 未保留 `TBD`、`TODO` 或“补充适当测试”等占位项。
- 类型一致性： 保持 `buildVtkPath(const std::string &, int, double)` 签名，避免改动 `PDEnginePost` 和积分器回调。
