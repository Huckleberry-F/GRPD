---
name: grpd-cae-toolkit
description: "GRPD 项目的系统型 CAE 开发工具包。用于材料本构、物理场链路、求解循环、OpenMP Kernel、YAML 输入、VTK 后处理、数值验证、CMake 构建、网格邻域和注册表工厂机制。触发时先运行 scripts 收集证据，再按 references 中的项目知识判断。"
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
- `references/performance-openmp.md`：OpenMP、邻域循环、缓存和数据竞争检查。
- `references/input-output-schema.md`：YAML 输入、Writer.Variables、VTK 输出。
- `references/cmake-layout.md`：CMake object library、依赖和新增源文件位置。
- `references/mesh-neighbor-contact.md`：MeshData、ParticleManager、NeighborList、ContactSpatialGrid。
- `references/validation-playbook.md`：CAE 最小验证算例与回归测试策略。

## Scripts

从仓库根目录运行：

```powershell
python .agent/skills/grpd-cae-toolkit/scripts/check_cmake_sources.py .
python .agent/skills/grpd-cae-toolkit/scripts/list_registry_macros.py .
python .agent/skills/grpd-cae-toolkit/scripts/check_field_references.py .
python .agent/skills/grpd-cae-toolkit/scripts/inspect_writer_variables.py .
python .agent/skills/grpd-cae-toolkit/scripts/validate_pd_yaml_basic.py Examples\Sphere\PD.yaml
```

脚本输出以 `[OK]`、`[WARN]`、`[MISSING_*]`、`[ERROR]` 标记。出现 `MISSING` 或 `ERROR` 时，应优先解释原因并给出最小修复。
