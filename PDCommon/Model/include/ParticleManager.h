#ifndef GRPD_MODEL_PARTICLE_MANAGER_H
#define GRPD_MODEL_PARTICLE_MANAGER_H

// ============================================================================
// ParticleManager.h - Global particle manager
// Responsibility: Holds the single std::vector<Particle> array, provides
//                 O(1) indexed access.
// Architecture rule: All particles must reside in this unique container only.
//                    Splitting by PartID is strictly forbidden!
// ============================================================================

#include "Particle.h"
#include <cstddef> // size_t
#include <string>
#include <vector>

namespace GRPD::Model {

enum class ModelVar { ID, Coords, Volume, PartID, MatID };

class ParticleManager {
public:
  ParticleManager() = default; // Public constructor for normal instantiation

  // -----------------------------------------------------------------------
  // Particle operation interface
  // -----------------------------------------------------------------------

  /// Emplace-construct a new particle and push it into the global array
  /// @param id     Globally unique ID (must equal current array size)
  /// @param partId Part number this particle belongs to
  /// @param matId  Material ID assigned to this particle
  /// @param x, y, z  3D coordinates
  /// @param volume Particle volume
  void addParticle(int id, int partId, int matId, double x, double y, double z,
                   double volume);

  /// O(1) indexed access - const version
  const Particle &getParticle(int id) const;

  /// O(1) indexed access - mutable version (e.g. for setting temperature)
  Particle &getParticle(int id);

  /// Get current total particle count
  size_t getTotalParticles() const;

  /// Pre-allocate memory before file reading to avoid dynamic resizing jitter
  /// @param size Expected total particle count
  void reserveMemory(size_t size);

  /// Get const reference to the underlying array (for solver traversal)
  const std::vector<Particle> &getAllParticles() const;

  /// Get mutable reference to the underlying array (for solver field writes)
  std::vector<Particle> &getAllParticles();

  // -----------------------------------------------------------------------
  // Dynamic Generic Extraction Routing
  // -----------------------------------------------------------------------
  double *getGeomDataPtr(ModelVar varName) const;
  int *getIntDataPtr(ModelVar varName) const;

  enum class VarDataType { Int, Double };

  /// Get the ModelVar enum and data type for a given variable string name
  static bool getModelVarInfo(const std::string &varName, ModelVar &outVar,
                              VarDataType &outType, int &outDim);

private:
  std::vector<Particle> globalParticles_; // The single global particle array
};

} // namespace GRPD::Model

#endif // GRPD_MODEL_PARTICLE_MANAGER_H
