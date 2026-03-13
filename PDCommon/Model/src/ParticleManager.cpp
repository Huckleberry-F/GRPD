// ============================================================================
// ParticleManager.cpp - Global particle manager implementation
// Responsibility: Implements in-place particle construction and memory
//                 pre-allocation
// ============================================================================

#include "ParticleManager.h"
#include "DataExtractor.h"
#include "Logger.h"
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace GRPD::Model {

// ---------------------------------------------------------------------------
// addParticle: Emplace-construct into the global array
// Safety check: Ensures the incoming ID matches the current array size
//               (enforcing the ID==index invariant)
// ---------------------------------------------------------------------------
void ParticleManager::addParticle(int id, int partId, int matId, double x,
                                  double y, double z, double volume) {
  // Assert ID continuity: incoming id must exactly equal current array length
  if (static_cast<size_t>(id) != globalParticles_.size()) {
    LOG_ERROR("[ParticleManager] Error: particle ID=" + std::to_string(id) +
              " does not match expected index " +
              std::to_string(globalParticles_.size()) +
              "! Violates zero-based ID invariant!");
    throw std::runtime_error(
        "Particle ID does not match expected vector index.");
  }

  globalParticles_.emplace_back(id, partId, matId, x, y, z, volume);
}

// ---------------------------------------------------------------------------
// reserveMemory: Pre-allocate memory to avoid repeated resizing overhead
// ---------------------------------------------------------------------------
void ParticleManager::reserveMemory(size_t size) {
  globalParticles_.reserve(size);
  LOG_INFO("[ParticleManager] Pre-allocated memory for " +
           std::to_string(size) + " particles");
}

// ---------------------------------------------------------------------------
// Getter implementations
// ---------------------------------------------------------------------------
const Particle &ParticleManager::getParticle(int id) const {
  return globalParticles_[id];
}

Particle &ParticleManager::getParticle(int id) { return globalParticles_[id]; }

size_t ParticleManager::getTotalParticles() const {
  return globalParticles_.size();
}

const std::vector<Particle> &ParticleManager::getAllParticles() const {
  return globalParticles_;
}

std::vector<Particle> &ParticleManager::getAllParticles() {
  return globalParticles_;
}

// ---------------------------------------------------------------------------
// Dynamic Generic Extraction Routing
// ---------------------------------------------------------------------------
double *ParticleManager::getGeomDataPtr(ModelVar varName) const {
  switch (varName) {
  case ModelVar::Coords:
    return Utils::DataExtractor::getVectorPtr<Particle, double, 3,
                                              &Particle::coords_>(
        globalParticles_);
  case ModelVar::Volume:
    return Utils::DataExtractor::getScalarPtr<Particle, double,
                                              &Particle::volume_>(
        globalParticles_);

  default:
    LOG_WARNING(
        "[ParticleManager] Requested unmapped double variable from ModelVar!");
    return nullptr;
  }
}

int *ParticleManager::getIntDataPtr(ModelVar varName) const {
  switch (varName) {
  case ModelVar::ID:
    return Utils::DataExtractor::getScalarPtr<Particle, int, &Particle::id_>(
        globalParticles_);
  case ModelVar::PartID:
    return Utils::DataExtractor::getScalarPtr<Particle, int,
                                              &Particle::partId_>(
        globalParticles_);
  case ModelVar::MatID:
    return Utils::DataExtractor::getScalarPtr<Particle, int, &Particle::matId_>(
        globalParticles_);
  default:
    LOG_WARNING(
        "[ParticleManager] Requested unmapped int variable from ModelVar!");
    return nullptr;
  }
}

bool ParticleManager::getModelVarInfo(const std::string &varName,
                                      ModelVar &outVar, VarDataType &outType,
                                      int &outDim) {
  // Map structure: { "StringName", { ModelVarEnum, VarDataTypeEnum,
  // DimensionValue } }
  struct VarInfo {
    ModelVar var;
    VarDataType type;
    int dim;
  };

  static const std::unordered_map<std::string, VarInfo> varMap = {
      {"Coords", {ModelVar::Coords, VarDataType::Double, 3}},
      {"Volume", {ModelVar::Volume, VarDataType::Double, 1}},

      {"ID", {ModelVar::ID, VarDataType::Int, 1}},
      {"PartID", {ModelVar::PartID, VarDataType::Int, 1}},
      {"MatID", {ModelVar::MatID, VarDataType::Int, 1}}};

  auto it = varMap.find(varName);
  if (it != varMap.end()) {
    outVar = it->second.var;
    outType = it->second.type;
    outDim = it->second.dim;
    return true;
  }
  return false;
}

} // namespace GRPD::Model
