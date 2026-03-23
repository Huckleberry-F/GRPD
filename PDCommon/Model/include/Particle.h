#ifndef PDCOMMON_MODEL_PARTICLE_H
#define PDCOMMON_MODEL_PARTICLE_H

// ============================================================================
// Particle.h - PD particle entity class
// Responsibility: Store geometric and physical field attributes of a single
//                 particle, providing zero-overhead inline access.
// Architecture rule: IDs start from 0 and map perfectly to the global
//                    std::vector index.
// ============================================================================

#include <array>

namespace PDCommon::Material {
class Material;
}

namespace PDCommon::Model {

class Particle {
  friend class ParticleManager;

public:
  // -----------------------------------------------------------------------
  // Constructor (uses member initializer list)
  // -----------------------------------------------------------------------
  Particle(int id, int partId, int matId, double x, double y, double z,
           double volume);

  // -----------------------------------------------------------------------
  // const Getters (implemented in Particle.cpp)
  // -----------------------------------------------------------------------

  /// Get the globally unique ID (equals the index in globalParticles_)
  int getId() const;

  /// Get the part number this particle belongs to
  int getPartId() const;

  /// Get const reference to the 3D coordinate array [x, y, z]
  const std::array<double, 3> &getCoords() const;

  /// Get the particle volume
  double getVolume() const;


  /// Get the material ID assigned from .grpd file
  int getMatId() const;

  /// Get the assigned physical material pointer
  ::PDCommon::Material::Material *getMaterial() const;

  // -----------------------------------------------------------------------
  // Setter (only temperature and material pointer are mutable)
  // -----------------------------------------------------------------------

  /// Assign the material instance to this particle
  void setMaterial(::PDCommon::Material::Material *mat);



private:
  int id_;                       // Globally unique ID (== vector index)
  int partId_;                   // Part number
  int matId_;                    // Material ID read from .grpd file
  std::array<double, 3> coords_; // 3D coordinates [x, y, z]
  double volume_;                // Particle volume

  PDCommon::Material::Material *material_ =
      nullptr; // Pointer to actual physical material instance
};

} // namespace PDCommon::Model

#endif // PDCOMMON_MODEL_PARTICLE_H
