---
name: grpd-cae-toolkit
description: "GRPD 项目的系统型 CAE 开发公共工具包。用于项目架构、注册表/工厂/管理器、材料状态变量、物理场链路、求解循环、YAML/输出共享链路和脚本化证据收集。专项知识由对应 skill 的 references 持有，触发时先运行 scripts 收集证据，再按必要 references 判断。"
---

# GRPD CAE Toolkit

## 工作原则

先收集证据，再改代码。优先运行本 skill 的脚本，读取必要 reference，然后按 `AGENTS.md` 的 C++17、模块解耦、注册表/工厂/管理器规则做最小修改。

## 推荐流程

1. 判断任务类型，选择下方 reference 和 script。
2. 运行相关脚本获取事实，不要只凭印象判断。
3. 阅读对应 reference，确认 GRPD 的真实链路。
4. 修改代码时保持头/源分离，不做无关重构。
5. 最后运行脚本或构建命令验证，并说明残余风险。

## References

- `references/architecture.md`：项目分层、初始化顺序、模块边界。
- `references/registry-factory.md`：Registry/Factory/Manager/Singleton 注册模式。
- `references/material-state.md`：材料参数、状态变量、Old/Trial、commit 规则。
- `references/field-pipeline.md`：PhysicsFields 到 FieldManager 的字段链路。
- `references/solver-loop.md`：PDEngine、TimeIntegrator、Kernel、BC、状态提交顺序。
- `references/input-output-schema.md`：YAML 输入、Writer.Variables、VTK 输出。

## Specialist References

- `../cmake-build-doctor/references/cmake-layout.md`：CMake object library、依赖和新增源文件位置。
- `../openmp-kernel-optimizer/references/performance-openmp.md`：OpenMP、邻域循环、缓存和数据竞争检查。
- `../mesh-and-neighbor-reviewer/references/mesh-neighbor-contact.md`：MeshData、ParticleManager、NeighborList、ContactSpatialGrid。
- `../numerical-test-generator/references/validation-playbook.md`：CAE 最小验证算例与回归测试策略。
- `../grpd-smoke-tester/references/smoke-test-playbook.md`：GRPD/ANSYS/path-comparison 冒烟测试闭环。

## Scripts

从仓库根目录运行：

```powershell
python .gemini/skills/grpd-cae-toolkit/scripts/check_cmake_sources.py .
python .gemini/skills/grpd-cae-toolkit/scripts/list_registry_macros.py .
python .gemini/skills/grpd-cae-toolkit/scripts/check_field_references.py .
python .gemini/skills/grpd-cae-toolkit/scripts/inspect_writer_variables.py .
python .gemini/skills/grpd-cae-toolkit/scripts/validate_pd_yaml_basic.py Examples\Sphere\PD.yaml
```

脚本输出以 `[OK]`、`[WARN]`、`[MISSING_*]`、`[ERROR]` 标记。出现 `MISSING` 或 `ERROR` 时，应优先解释原因并给出最小修复。
