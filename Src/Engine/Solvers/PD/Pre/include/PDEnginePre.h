// ============================================================================
// PDEngineInit.h - 引擎阶段初始化自由函数集合 (去类化结构)
// ============================================================================

#ifndef SRC_ENGINE_SOLVERS_PD_INIT_PDENGINE_INIT_H
#define SRC_ENGINE_SOLVERS_PD_INIT_PDENGINE_INIT_H

#include "PDContext.h"
#include "PDKernel.h"
#include "TimeIntegrator.h"
#include <memory>
#include <string>
#include <vector>
#include <yaml-cpp/yaml.h>

namespace Src::Engine::Solvers::PD::Init {

/// @brief 初始化模型（包含 Python 网格生成与多态 Reader 读取）
/// @details 从 yaml 或已有 grpd 获取网格拓扑，并将之转化为 Particle 对象送入 ParticleManager。
/// @param ctx PD 全局上下文
/// @param config 模拟系统的 yaml 根配置
/// @param yamlPath 运行所用的 yaml 文件绝对路径
void InitModel(PDCommon::Core::PDContext &ctx, const YAML::Node &config,
               const std::string &yamlPath);

/// @brief 初始化材料本构分配
/// @details 通过 MaterialRegistry 动态解析 yaml 所需本构模型类型并分发给所有粒子。
/// @param ctx PD 全局上下文
/// @param config 模拟系统的 yaml 根配置
void InitMaterial(PDCommon::Core::PDContext &ctx, const YAML::Node &config);

/// @brief 注册系统的核心物理场及 SDV(状态变量场)
/// @details 提取物理问题类型 (例如 Thermal) 并由 PhysicsFieldRegistry 自动在 FieldManager 中开辟并填充几何场、状态场。
/// @param ctx PD 全局上下文
/// @param config 模拟系统的 yaml 根配置
void InitFields(PDCommon::Core::PDContext &ctx, const YAML::Node &config);

/// @brief 初始化并推导边界条件
/// @details 读取 *LOAD 等载荷节，生成多态 BC 子类，并转译到物理节点施加源项约束。
/// @param ctx PD 全局上下文
/// @param config 模拟系统的 yaml 根配置
void InitConditions(PDCommon::Core::PDContext &ctx, const YAML::Node &config);

/// @brief 构建模型近邻关系表
/// @details 基于给定的 Horizon 半径利用 Cell List 将近邻绑定成键 (Bond)，并注入 NeighborCount 和 BondDamage 场。
/// @param ctx PD 全局上下文
/// @param config 模拟系统的 yaml 根配置
void InitNeighbors(PDCommon::Core::PDContext &ctx, const YAML::Node &config);

/// @brief 初始化所有带有私有 Damage 准则的 Material（需在拓扑建树之后执行）
/// @details 为这些存在断裂模型的材质预分配状态缓存（如初始完好键统计）。
/// @param ctx PD 全局上下文
/// @param config 模拟系统的 yaml 根配置
void InitFractureModels(PDCommon::Core::PDContext &ctx, const YAML::Node &config);

/// @brief 初始化宏观裂纹挂载
/// @details 读取 PreCracks 配置并用几何算法切断对应债券，形成预置裂纹面或几何缝。
/// @param ctx PD 全局上下文
/// @param config 模拟系统的 yaml 根配置
void InitPreCracks(PDCommon::Core::PDContext &ctx, const YAML::Node &config);

/// @brief 初始化接触算法管线
/// @details 根据 YAML 中 Solver.Contact 决定是否加载全局 PenaltyContact 算法并注入 Context。
/// @param ctx PD 全局上下文
/// @param config 模拟系统的 yaml 根配置
void InitContact(PDCommon::Core::PDContext &ctx, const YAML::Node &config);

/// @brief 初始化后处理挂载组件
/// @details 根据 YAML 中 PostProcessing 节点加载用户自定义后处理算子，并送入 PostProcessorManager。
/// @param ctx PD 全局上下文
/// @param config 模拟系统的 yaml 根配置
void InitPostProcessing(PDCommon::Core::PDContext &ctx, const YAML::Node &config);

/// @brief 初始化微积分方程执行器与空间积分内核（多核版本）
/// @details 在 L1 和 L2 的架构层组装出 TimeIntegrator（如 ExplicitEuler）和多个 PDKernel 内核。
///          支持 YAML 中的 Solver.Kernel（单核兼容）和 Solver.Kernels（多核列表）两种模式。
/// @param config 模拟系统的 yaml 根配置
/// @param integrator 传出装配好的时间积分器对象
/// @param kernels 传出装配好的空间微积分内核集合
void InitSolverComponents(
    const YAML::Node &config,
    std::unique_ptr<Src::Integration::TimeIntegrator> &integrator,
    std::vector<std::unique_ptr<PDCommon::Kernel::PDKernel>> &kernels);

} // namespace Src::Engine::Solvers::PD::Init

#endif // SRC_ENGINE_SOLVERS_PD_INIT_PDENGINE_INIT_H
