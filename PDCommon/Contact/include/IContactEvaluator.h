#ifndef PDCOMMON_CONTACT_ICONTACTEVALUATOR_H
#define PDCOMMON_CONTACT_ICONTACTEVALUATOR_H

#include "IContactForceLaw.h"
#include <yaml-cpp/yaml.h>
#include <string>
#include <vector>

namespace PDCommon::Core { class PDContext; }

namespace PDCommon::Contact {
class ContactSpatialGrid;

/// @brief 接触几何与运动学评判器接口 (Axis B)
/// @details 负责将 Axis A (Search Grid) 给出的周围点群，提取为
///          统一的几何与运动学信息（法向、穿透量等），生成 ContactContext。
class IContactEvaluator {
public:
  virtual ~IContactEvaluator() = default;

  /// @brief 获取评判器类型名（如 "NTNEvaluator", "NTCEvaluator"）
  virtual std::string getTypeName() const = 0;

  /// @brief 获取该 Evaluator 需要的扩大网格搜索半径系数，默认 1.0 (NTN/NTC 够用)
  virtual double getSearchRadiusRatio() const { return 1.0; }

  /// @brief 从 YAML 初始化参数（如果需要，如 pinballRatio）
  virtual void initialize(const YAML::Node &configNode) = 0;

  /// @brief 生命周期预处理（在评价所有从面节点之前调用）
  virtual void onPreEvaluate(PDCommon::Core::PDContext &ctx, double maxDx) {}

  /// @brief 传入指定的从面粒子，利用搜索网格提取出一个或多个接触上下文
  /// @param slaveId 待评价的从面粒子 ID
  /// @param grid 建立好的从面包围体空间网格
  /// @param ctx 全局状态上下文
  /// @return 提取出来的 ContactContext 列表（传给 C,D轴进行受力计算）
  virtual std::vector<ContactContext> evaluate(
      int slaveId, const ContactSpatialGrid &grid, PDCommon::Core::PDContext &ctx) = 0;

  /// @brief 设置质量缩放因子，同步自外层 GeneralContact
  virtual void setMassScaleFactor(double sf) {}
};

} // namespace PDCommon::Contact

#endif // PDCOMMON_CONTACT_ICONTACTEVALUATOR_H
