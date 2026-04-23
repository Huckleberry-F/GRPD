#ifndef PDCOMMON_CONTACT_ICONTACTALGORITHM_H
#define PDCOMMON_CONTACT_ICONTACTALGORITHM_H

// ============================================================================
// IContactAlgorithm.h - 接触对基类（对标 Material / BC 基类）
// 责任：
// 1. 定义一个接触对（Contact Pair）的完整描述：
//    - 主面/从面的粒子集合
//    - 接触参数（罚刚度、摩擦系数等）
//    - 接触力计算逻辑 (computeContactForce)
// 2. 不同接触类型（Penalty、Friction 等）继承此基类，
//    通过 ContactRegistry 进行工厂注册，对标 BC 子类的注册方式。
// ============================================================================

#include <string>
#include <vector>
#include <yaml-cpp/yaml.h>

namespace PDCommon::Core { class PDContext; }

namespace PDCommon::Contact {

/// @brief 接触对基类（对标 Material / BC）
/// @details 每个实例代表一组接触对定义（如 bullet-target），
///          包含主从面定义、接触参数，以及具体的接触力计算算法。
///          不同算法类型（Penalty、Friction）通过继承此基类实现。
class IContactAlgorithm {
public:
  /// @brief 构造函数
  /// @param name 接触对实例名称（如 "bullet_target_contact"）
  explicit IContactAlgorithm(const std::string &name = "") : name_(name) {}
  virtual ~IContactAlgorithm() = default;

  // 禁用拷贝
  IContactAlgorithm(const IContactAlgorithm &) = delete;
  IContactAlgorithm &operator=(const IContactAlgorithm &) = delete;

  // -----------------------------------------------------------------------
  // 基本属性访问
  // -----------------------------------------------------------------------

  /// @brief 获取接触对名称
  const std::string &getName() const { return name_; }

  /// @brief 设置接触对名称
  void setName(const std::string &name) { name_ = name; }

  /// @brief 获取接触算法类型名（如 "Penalty"、"Friction"）
  virtual std::string getTypeName() const = 0;

  // -----------------------------------------------------------------------
  // 生命周期接口
  // -----------------------------------------------------------------------

  /// @brief 从 YAML 配置节点初始化接触对参数
  /// @details 解析主从面定义、罚刚度、摩擦系数等参数
  /// @param configNode 对应于本接触对的 YAML 配置节点
  virtual void initialize(const YAML::Node &configNode) = 0;

  /// @brief 计算接触力并注入到全局力场中
  /// @param ctx 包含所有粒子、场、邻域等信息的上下文
  virtual void computeContactForce(PDCommon::Core::PDContext &ctx) = 0;

  // -----------------------------------------------------------------------
  // 主从面粒子管理（公共接口）
  // -----------------------------------------------------------------------

  /// @brief 获取主面（Master/Target）粒子 ID 列表
  const std::vector<int> &getMasterParticleIds() const { return masterIds_; }

  /// @brief 获取从面（Slave/Contact）粒子 ID 列表
  const std::vector<int> &getSlaveParticleIds() const { return slaveIds_; }

  /// @brief 设置主面粒子 ID 列表
  void setMasterParticleIds(const std::vector<int> &ids) { masterIds_ = ids; }

  /// @brief 设置从面粒子 ID 列表
  void setSlaveParticleIds(const std::vector<int> &ids) { slaveIds_ = ids; }

  /// @brief 设置质量缩放因子，用于显式计算时的正确加速度映射
  virtual void setMassScaleFactor(double sf) { massScaleFactor_ = sf; }

protected:
  std::string name_;               ///< 接触对实例名称
  std::vector<int> masterIds_;     ///< 主面（Target）粒子 ID 集合
  std::vector<int> slaveIds_;      ///< 从面（Contact）粒子 ID 集合
  double massScaleFactor_ = 1.0;   ///< 质量放大系数
};

} // namespace PDCommon::Contact

#endif // PDCOMMON_CONTACT_ICONTACTALGORITHM_H
