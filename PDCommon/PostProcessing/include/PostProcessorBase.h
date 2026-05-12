#ifndef PDCOMMON_POSTPROCESSING_POSTPROCESSORBASE_H
#define PDCOMMON_POSTPROCESSING_POSTPROCESSORBASE_H

#include <yaml-cpp/yaml.h>
#include <string>

// Forward declaration
namespace PDCommon::Core {
class PDContext;
}

namespace PDCommon::PostProcessing {

/**
 * @brief 后处理算子基类
 * 提供在求解过程各个生命周期的挂载点（Hooks）
 */
class PostProcessorBase {
public:
  virtual ~PostProcessorBase() = default;

  // 初始化接口，解析专属的 yaml 参数并分配所需物理场
  virtual void initialize(PDCommon::Core::PDContext &ctx,
                          const YAML::Node &config) {}

  // 钩子1：在边界条件 (BC) 清零加速度/覆盖位移之前触发
  // 适合用于截获真实支反力等
  virtual void preBCEvaluate(PDCommon::Core::PDContext &ctx, double currentTime,
                             int stepId) {}

  // 钩子2：在整个物理积分步/子步彻底结束、所有场完成 Swap 之后触发
  // 适合用于计算应变能、统计断裂率、生成额外云图等
  virtual void postStepEvaluate(PDCommon::Core::PDContext &ctx,
                                double currentTime, int stepId) {}
};

} // namespace PDCommon::PostProcessing

#endif // PDCOMMON_POSTPROCESSING_POSTPROCESSORBASE_H
