#ifndef PDCOMMON_POSTPROCESSING_POSTPROCESSORMANAGER_H
#define PDCOMMON_POSTPROCESSING_POSTPROCESSORMANAGER_H

#include "PostProcessorBase.h"
#include <memory>
#include <vector>

namespace PDCommon::Core {
class PDContext;
}

namespace PDCommon::PostProcessing {

class PostProcessorManager {
public:
  PostProcessorManager() = default;
  ~PostProcessorManager() = default;

  // 注册并初始化从 YAML 解析出的后处理算子
  void addPostProcessor(std::unique_ptr<PostProcessorBase> pp);

  // 初始化所有绑定的算子
  void initializeAll(PDCommon::Core::PDContext &ctx, const YAML::Node &configNode);

  // 执行所有算子的 PreBC 钩子
  void executePreBCHooks(PDCommon::Core::PDContext &ctx, double currentTime, int stepId);

  // 执行所有算子的 PostStep 钩子
  void executePostStepHooks(PDCommon::Core::PDContext &ctx, double currentTime, int stepId);

private:
  std::vector<std::unique_ptr<PostProcessorBase>> processors_;
};

} // namespace PDCommon::PostProcessing

#endif // PDCOMMON_POSTPROCESSING_POSTPROCESSORMANAGER_H
