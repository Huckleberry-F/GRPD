// ============================================================================
// DamageModel.h - 损伤/断裂模抽象基类
// ============================================================================

#ifndef PDCOMMON_DAMAGE_DAMAGEMODEL_H
#define PDCOMMON_DAMAGE_DAMAGEMODEL_H

#include <yaml-cpp/yaml.h>
#include <string>

namespace PDCommon::Core {
class PDContext;
}

namespace PDCommon::Damage {

/// @brief 损伤评估抽象基类
/// 提供统一接口供近场动力学核心 (PDKernel) 在适当的时机调用。
class DamageModel {
public:
  virtual ~DamageModel() = default;

  /// @brief 从 YAML 加载模型参数
  virtual void configure(const YAML::Node &node) = 0;

  /// @brief 初始化 / 预计算 (在第一个时间步前被调用一次)
  /// 用于向 NeighborList 注册额外的 BondFields，或向 FieldManager 注册全局 Damage 场。
  virtual void preCompute(PDCommon::Core::PDContext &ctx, int matId = -1) = 0;

  /// @brief 计算损伤 / 判定键断裂
  /// @details 通常在 postCompute 阶段调用，计算键的当前状态（伸长率/应力等），
  /// 若达到失效条件，则置对应的 neighbour ID 为 -1。
  virtual void computeDamage(PDCommon::Core::PDContext &ctx, int matId = -1) = 0;

  /// @brief 返回本损伤模型的名字 (用于识别)
  virtual std::string getName() const = 0;

protected:
  DamageModel() = default;
};

} // namespace PDCommon::Damage

#endif // PDCOMMON_DAMAGE_DAMAGEMODEL_H
