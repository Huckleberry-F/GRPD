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
  /// 基类提供了公用的场申请与初始键统计逻辑，子类可覆盖但建议在开头调用基类的 preCompute。
  virtual void preCompute(PDCommon::Core::PDContext &ctx, int matId = -1);

  /// @brief 计算损伤 / 判定键断裂
  virtual void computeDamage(PDCommon::Core::PDContext &ctx, int matId = -1) = 0;

  /// @brief 返回本损伤模型的名字 (用于识别)
  virtual std::string getName() const = 0;

protected:
  DamageModel() = default;

  // 全局缓存的每个节点的初始键数，由基类的 preCompute 统一分配和统计
  std::vector<int> initialBondsCount_;

  // 辅助函数：依据当前 activeBonds 安全地计算出 0~1 的损伤标量
  inline double calculateDamageRatio(int particleIndex, int activeBonds) const {
      if (initialBondsCount_.empty() || initialBondsCount_[particleIndex] <= 0) return 0.0;
      return 1.0 - static_cast<double>(activeBonds) / static_cast<double>(initialBondsCount_[particleIndex]);
  }
};

} // namespace PDCommon::Damage

#endif // PDCOMMON_DAMAGE_DAMAGEMODEL_H
