// ============================================================================
// PDKernel.h - PD 积分核心抽象基类 (Layer 2)
//
// 职责：
//   定义所有 PD 非局部积分框架（NOSB / BB / OSB）的统一接口。
//   TimeIntegrator (L1) 通过此接口调用 PD 核心积分，
//   实现算法层与 PD 理论层的完全解耦。
//
// 架构位置：
//   PDEngine::Solve()
//     → integrator_->run()          ← L1: 求解算法
//         → kernel_->computeForceState()  ← L2: 本类
//             → mat->evaluate()     ← L3: 本构
// ============================================================================

#ifndef PDCOMMON_KERNEL_PDKERNEL_H
#define PDCOMMON_KERNEL_PDKERNEL_H

#include "PDContext.h"
#include <string>
#include <vector>
#include <yaml-cpp/yaml.h>

namespace PDCommon::Kernel {

/// @brief 影响函数（核函数）权重类型
enum class InfluenceKernelType {
  Constant,
  InverseDistance,
  Linear,
  Quadratic,
  Cubic,
  Quartic,
  Gaussian
};

class PDKernel {
public:
  virtual ~PDKernel() = default;

  // 禁用拷贝
  PDKernel(const PDKernel &) = delete;
  PDKernel &operator=(const PDKernel &) = delete;

  // -----------------------------------------------------------------------
  // 核心接口
  // -----------------------------------------------------------------------

  /// @brief 从 YAML Solver 段读取本核心的配置参数
  /// 默认空实现，各中间基类/子类按需重写
  virtual void configure(const YAML::Node &solverNode) {}

  /// @brief 时间循环前的一次性预计算（如 NOSB 形函数张量）
  /// @param ctx PD 仿真上下文
  virtual void preCompute(PDCommon::Core::PDContext &ctx) = 0;

  /// @brief 每个时间步的核心积分计算
  /// @param ctx PD 仿真上下文
  virtual void computeForceState(PDCommon::Core::PDContext &ctx) = 0;

  /// @brief 时间步积分完成后的善后钩子（可选重写）
  /// @details 供子类在时间推进结束后执行状态变量演化、热应变更新、
  ///          材料参数温度依赖刷新等后处理操作。默认空实现。
  /// @param ctx PD 仿真上下文
  virtual void postCompute(PDCommon::Core::PDContext &ctx) {}

  // -----------------------------------------------------------------------
  // 时间积分辅助信息
  // -----------------------------------------------------------------------

  /// @brief 描述一个需要时间积分的场对
  struct IntegrationTarget {
    std::string primaryField; ///< 被更新的主场 (如 "Temperature")
    std::string rateField;    ///< 变化率场 (如 "TempRate")
    int dimension;            ///< 场的分量数 (标量=1, 向量=3)
  };

  /// @brief 返回此核心需要时间积分的场列表
  virtual std::vector<IntegrationTarget> getIntegrationTargets() const = 0;

protected:
  PDKernel() = default; // 只允许子类构造
  
  InfluenceKernelType kernelType_{InfluenceKernelType::Constant};

  /// @brief 获取指定核函数（影响函数）的权重值
  inline double GetInfluenceWeight(double xi, double horizon,
                            InfluenceKernelType type) const {
    switch (type) {
    case InfluenceKernelType::InverseDistance:
      return (xi > 1e-16) ? (1.0 / xi) : 0.0;
    case InfluenceKernelType::Constant:
      return 1.0;
    case InfluenceKernelType::Linear: {
      double r = 1.0 - xi / horizon;
      return (r > 0.0) ? r : 0.0;
    }
    case InfluenceKernelType::Quadratic: {
      double r = 1.0 - xi / horizon;
      return (r > 0.0) ? r * r : 0.0;
    }
    case InfluenceKernelType::Cubic: {
      double r = 1.0 - xi / horizon;
      return (r > 0.0) ? r * r * r : 0.0;
    }
    case InfluenceKernelType::Quartic: {
      double r = 1.0 - xi / horizon;
      return (r > 0.0) ? r * r * r * r : 0.0;
    }
    case InfluenceKernelType::Gaussian:
      return std::exp(-(xi * xi) / (horizon * horizon));
    default:
      return 1.0;
    }
  }
};

} // namespace PDCommon::Kernel

#endif // PDCOMMON_KERNEL_PDKERNEL_H
