#ifndef PDCOMMON_BC_THERMAL_BC_H
#define PDCOMMON_BC_THERMAL_BC_H

// ============================================================================
// ThermalBC.h - 热边界条件派生体系
// 责任：
// 1. ThermalBC 作为热边界条件的中间抽象类，继承 BC 基类
// 2. 派生出 TemperatureBC (Dirichlet), HeatFluxBC (Neumann), ConvectionBC
// (Robin)
// 3. 采用稀疏存储粒子 ID，避免全量扫描
// 架构规约：通过 REGISTER_BC_TYPE 宏在编译期自动注册到 BCRegistry
// ============================================================================

#include "BC.h"
#include "TypedField.h"

namespace PDCommon::BC {

/**
 * @brief 热边界条件抽象中间类
 * 持有三个 TypedField<double> 指针（温度、温度变化率、热流密度），
 * 所有热相关 BC 均从此类派生。
 * 使用指针而非引用以支持工厂模式的两阶段构造。
 */
class ThermalBC : public BC {
public:
  /// 工厂构造：仅传入名称，field 指针在 initialize() 中注入
  explicit ThermalBC(const std::string &name)
      : BC(name), temperature_(nullptr), tempRate_(nullptr),
        heatFlux_(nullptr) {}

  virtual ~ThermalBC() = default;

protected:
  PDCommon::Field::TypedField<double> *temperature_; // 温度场指针
  PDCommon::Field::TypedField<double> *tempRate_;    // 温度变化率场指针
  PDCommon::Field::TypedField<double> *heatFlux_;    // 热流密度场指针
};

/**
 * @brief 温度边界条件 (Dirichlet)
 * T(x, t) = T_fixed
 */
class TemperatureBC : public ThermalBC {
public:
  /// 工厂构造函数：仅传入名称
  explicit TemperatureBC(const std::string &name)
      : ThermalBC(name), particleId_(-1), temperatureVal_(0.0), prevVal_(0.0) {}

  /// 初始化：从 FieldManager 获取场指针，设置粒子 ID 和温度值
  void initialize(PDCommon::Field::FieldManager &fieldManager, int particleId,
                  const std::vector<double> &values) override;

  void apply() override;
  void apply(double loadFactor) override;
  void commitEndStep() override;
  bool isConstraint() const override { return true; }
  int getParticleId() const override { return particleId_; }

private:
  int particleId_;
  double temperatureVal_;
  double prevVal_;
};

/**
 * @brief 热流边界条件 (Neumann)
 * q = q_fixed
 */
class HeatFluxBC : public ThermalBC {
public:
  /// 工厂构造函数
  explicit HeatFluxBC(const std::string &name)
      : ThermalBC(name), particleId_(-1), baseFlux_(0.0), flux_(0.0), prevVal_(0.0) {}

  /// 初始化：从 FieldManager 获取场指针，设置粒子 ID 和热流密度
  void initialize(PDCommon::Field::FieldManager &fieldManager, int particleId,
                  const std::vector<double> &values) override;

  void apply() override;
  void apply(double loadFactor) override;
  void commitEndStep() override;
  void setScalingFactors(double dx, double density, double massScale, int dim = 3, double thickness = 1.0) override;
  bool isConstraint() const override {
    return false;
  } // Neumann: 向变化率场累加

private:
  int particleId_;
  double baseFlux_;
  double flux_;
  double prevVal_;
  double vol_ = 1.0;
};

/**
 * @brief 对流边界条件 (Robin)
 * q = h * (T_inf - T)
 */
class ConvectionBC : public ThermalBC {
public:
  /// 工厂构造函数
  explicit ConvectionBC(const std::string &name)
      : ThermalBC(name), particleId_(-1), baseHConv_(0.0), hConv_(0.0), tInf_(0.0),
        prevTInf_(0.0) {}

  /// 初始化：从 FieldManager 获取场指针，设置粒子 ID、h 和 T_inf
  void initialize(PDCommon::Field::FieldManager &fieldManager, int particleId,
                  const std::vector<double> &values) override;

  void apply() override;
  void apply(double loadFactor) override;
  void commitEndStep() override;
  void setScalingFactors(double dx, double density, double massScale, int dim = 3, double thickness = 1.0) override;
  bool isConstraint() const override { return false; } // Robin: 向变化率场累加

private:
  int particleId_;
  double baseHConv_;
  double hConv_;
  double tInf_;
  double prevTInf_; // tInf 随时间放缩
  double vol_ = 1.0;
};

} // namespace PDCommon::BC

#endif // PDCOMMON_BC_THERMAL_BC_H
