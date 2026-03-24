#ifndef PDCOMMON_KERNEL_STABILIZER_REGISTRY_H
#define PDCOMMON_KERNEL_STABILIZER_REGISTRY_H

#include "Stabilizer.h"
#include <functional>
#include <map>
#include <memory>
#include <string>

namespace PDCommon::Kernel {

/// @brief 稳定器工厂创建函数别名
using StabilizerCreator = std::function<std::unique_ptr<Stabilizer>()>;

/// @brief 稳定器全局注册表 (Singleton Factory)
/// 用于编译期静态注册所有 Stabilizer 类型，并在运行时通过 YAML 字符串名称安全实例化。
class StabilizerRegistry {
public:
  static StabilizerRegistry &getInstance();

  /// @brief 将名称映射绑定到具体的工厂创建器
  void registerStabilizer(const std::string &name, StabilizerCreator creator);

  /// @brief 通过字符串名称多态创建唯一的稳定器实例
  std::unique_ptr<Stabilizer> create(const std::string &name) const;

  /// @brief 检查一个稳定器是否已注册
  bool contains(const std::string &name) const;

private:
  StabilizerRegistry() = default;
  ~StabilizerRegistry() = default;
  StabilizerRegistry(const StabilizerRegistry &) = delete;
  StabilizerRegistry &operator=(const StabilizerRegistry &) = delete;

  std::map<std::string, StabilizerCreator> registryMap_;
};

/// @brief 辅助工具类：帮助在源文件中执行编译期自我静态注册
class StabilizerRegistrar {
public:
  StabilizerRegistrar(const std::string &name, StabilizerCreator creator) {
    StabilizerRegistry::getInstance().registerStabilizer(name, creator);
  }
};

} // namespace PDCommon::Kernel

// -------------------------------------------------------------------------
// 注册宏代码：在具体的 Stabilizer.cpp 中调用，以静态创建全局反射实例
// 示例：REGISTER_STABILIZER(Thermal_Zhang, PDCommon::Kernel::ZhangThermalStabilizer)
// -------------------------------------------------------------------------
#define REGISTER_STABILIZER(TypeName, ClassName)                               \
  static PDCommon::Kernel::StabilizerRegistrar                                 \
      registrar_##TypeName(#TypeName, []() {                                   \
        auto inst = std::make_unique<ClassName>();                             \
        inst->setName(#TypeName);                                              \
        return inst;                                                           \
      });

#endif // PDCOMMON_KERNEL_STABILIZER_REGISTRY_H
