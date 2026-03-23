#ifndef PDCOMMON_IO_READER_REGISTRY_H
#define PDCOMMON_IO_READER_REGISTRY_H

// ============================================================================
// ReaderRegistry.h - 网格读取器全局注册中心 (单例)
//
// 架构对称于 MaterialRegistry：
//   MaterialRegistry:  registerMaterialType(type, creator) + createMaterial()
//   ReaderRegistry:    registerReaderType(ext, creator) + createReader()
//
// 唯一区别：注册键是文件后缀名（如 ".grpd", ".inp"），而非类型名。
//
// 新增解析器只需：
//   1. 写一个继承 MeshReader 的子类
//   2. 在 .cpp 末尾加一行：REGISTER_READER(".xxx", XxxMeshReader)
//   零修改注册中心代码，彻底消除 if-else 分支。
// ============================================================================

#include "MeshReader.h"
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace PDCommon::IO {

/// 工厂函数类型：返回 MeshReader 的 unique_ptr（对称 MaterialCreatorFunc）
using ReaderCreatorFunc =
    std::function<std::unique_ptr<MeshReader>(const std::string &)>;

class ReaderRegistry {
public:
  /// @brief 获取单例
  static ReaderRegistry &getInstance();

  // 禁用拷贝
  ReaderRegistry(const ReaderRegistry &) = delete;
  ReaderRegistry &operator=(const ReaderRegistry &) = delete;

  /// @brief 注册读取器类型（对称 registerMaterialType）
  /// @param extension 文件后缀名（含点号，如 ".grpd", ".inp"）
  /// @param creator 工厂函数
  void registerReaderType(const std::string &extension,
                          ReaderCreatorFunc creator);

  /// @brief 根据后缀名创建读取器实例（对称 createMaterial）
  /// @param extension 文件后缀名
  /// @param name 实例名称
  /// @return MeshReader 实例的 unique_ptr，未注册则返回 nullptr
  std::unique_ptr<MeshReader> createReader(const std::string &extension,
                                           const std::string &name);

  /// @brief 检查某后缀名是否已注册
  bool hasType(const std::string &extension) const;

  /// @brief 获取所有已注册的后缀名列表
  std::vector<std::string> getRegisteredTypes() const;

private:
  ReaderRegistry() = default;
  ~ReaderRegistry() = default;

  std::map<std::string, ReaderCreatorFunc> registry_;
};

// ---------------------------------------------------------------------------
// 自动注册宏（对称 REGISTER_MATERIAL_TYPE）
// 用法（在 .cpp 文件末尾，namespace 内部）：
//   REGISTER_READER(".grpd", GrpdMeshReader)
//   REGISTER_READER(".inp",  InpMeshReader)
// ---------------------------------------------------------------------------
#define REGISTER_READER(Extension, Class)                                      \
  namespace {                                                                  \
  struct ReaderRegistrar_##Class {                                             \
    ReaderRegistrar_##Class() {                                                \
      PDCommon::IO::ReaderRegistry::getInstance().registerReaderType(          \
          Extension, [](const std::string &name) {                            \
            return std::make_unique<Class>(name);                              \
          });                                                                  \
    }                                                                          \
  };                                                                           \
  static ReaderRegistrar_##Class global_ReaderRegistrar_##Class;               \
  }

} // namespace PDCommon::IO

#endif // PDCOMMON_IO_READER_REGISTRY_H
