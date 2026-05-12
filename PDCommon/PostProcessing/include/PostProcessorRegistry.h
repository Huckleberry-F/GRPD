#ifndef PDCOMMON_POSTPROCESSING_POSTPROCESSORREGISTRY_H
#define PDCOMMON_POSTPROCESSING_POSTPROCESSORREGISTRY_H

#include "PostProcessorBase.h"
#include <functional>
#include <map>
#include <memory>
#include <string>

namespace PDCommon::PostProcessing {

// Creator function type
using PostProcessorCreator = std::function<std::unique_ptr<PostProcessorBase>()>;

class PostProcessorRegistry {
public:
  static PostProcessorRegistry &getInstance();

  void registerCreator(const std::string &type, PostProcessorCreator creator);
  std::unique_ptr<PostProcessorBase> create(const std::string &type) const;

private:
  PostProcessorRegistry() = default;
  ~PostProcessorRegistry() = default;
  PostProcessorRegistry(const PostProcessorRegistry &) = delete;
  PostProcessorRegistry &operator=(const PostProcessorRegistry &) = delete;

  std::map<std::string, PostProcessorCreator> creators_;
};

// Registration macro
#define REGISTER_POSTPROCESSOR(Type, ClassName)                                \
  namespace {                                                                  \
  struct PostProcessorRegistrar_##Type {                                       \
    PostProcessorRegistrar_##Type() {                                          \
      PDCommon::PostProcessing::PostProcessorRegistry::getInstance()           \
          .registerCreator(#Type, []() {                                       \
            return std::make_unique<ClassName>();                              \
          });                                                                  \
    }                                                                          \
  };                                                                           \
  static PostProcessorRegistrar_##Type global_PostProcessorRegistrar_##Type;   \
  }

} // namespace PDCommon::PostProcessing

#endif // PDCOMMON_POSTPROCESSING_POSTPROCESSORREGISTRY_H
