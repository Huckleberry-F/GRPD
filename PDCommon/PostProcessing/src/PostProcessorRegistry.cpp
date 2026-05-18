#include "PostProcessorRegistry.h"
#include <iostream>

namespace PDCommon::PostProcessing {

PostProcessorRegistry &PostProcessorRegistry::getInstance() {
  static PostProcessorRegistry instance;
  return instance;
}

void PostProcessorRegistry::registerCreator(const std::string &type,
                                            PostProcessorCreator creator) {
  if (creators_.find(type) != creators_.end()) {
    std::cerr << "[PostProcessorRegistry] Warning: Overwriting creator for "
                 "PostProcessor type: "
              << type << "\n";
  }
  creators_[type] = std::move(creator);
}

std::unique_ptr<PostProcessorBase>
PostProcessorRegistry::create(const std::string &type) const {
  auto it = creators_.find(type);
  if (it != creators_.end()) {
    return it->second();
  }
  std::cerr
      << "[PostProcessorRegistry] Error: Unknown PostProcessor type requested: "
      << type << "\n";
  return nullptr;
}

} // namespace PDCommon::PostProcessing
