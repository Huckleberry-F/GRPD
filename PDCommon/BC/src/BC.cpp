// ============================================================================
// BC.cpp - 边界条件基类实现
// ============================================================================

#include "BC.h"

namespace GRPD::BC {

BC::BC(const std::string &name) : name_(name) {}

const std::string &BC::getName() const { return name_; }

void BC::setName(const std::string &name) { name_ = name; }

} // namespace GRPD::BC
