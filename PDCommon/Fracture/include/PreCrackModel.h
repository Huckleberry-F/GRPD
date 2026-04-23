#ifndef PDCOMMON_FRACTURE_PRECRACKMODEL_H
#define PDCOMMON_FRACTURE_PRECRACKMODEL_H

#include <yaml-cpp/yaml.h>
#include "PDContext.h"
#include <string>

namespace PDCommon::Fracture {

/// @brief 预置裂纹抽象基类
class PreCrackModel {
public:
  virtual ~PreCrackModel() = default;

  /// @brief 配置裂纹几何参数
  virtual void configure(const YAML::Node &node) = 0;

  /// @brief 应用预置裂纹：扫描相邻列表并打断被裂纹切割的键
  virtual void apply(PDCommon::Core::PDContext &ctx) = 0;

  const std::string& getType() const { return type_; }
  void setType(const std::string& type) { type_ = type; }

protected:
  std::string type_;
};

} // namespace PDCommon::Fracture

#endif // PDCOMMON_FRACTURE_PRECRACKMODEL_H
