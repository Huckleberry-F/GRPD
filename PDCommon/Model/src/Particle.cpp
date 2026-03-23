// ============================================================================
// Particle.cpp - PD particle entity class implementation
// Responsibility: Implements constructor and accessors
// ============================================================================

#include "Particle.h"

namespace PDCommon::Model {

// ---------------------------------------------------------------------------
// Constructor: uses member initializer list, zero redundant assignment
// ---------------------------------------------------------------------------
Particle::Particle(int id, int partId, int matId, double x, double y, double z,
                   double volume)
    : id_(id), partId_(partId), matId_(matId), coords_({x, y, z}),
      volume_(volume), material_(nullptr) {}

// ---------------------------------------------------------------------------
// Getter implementations
// ---------------------------------------------------------------------------
int Particle::getId() const { return id_; }

int Particle::getPartId() const { return partId_; }

const std::array<double, 3> &Particle::getCoords() const { return coords_; }

double Particle::getVolume() const { return volume_; }



int Particle::getMatId() const { return matId_; }

::PDCommon::Material::Material *Particle::getMaterial() const { return material_; }

// ---------------------------------------------------------------------------
// Setter implementations
// ---------------------------------------------------------------------------


void Particle::setMaterial(::PDCommon::Material::Material *mat) { material_ = mat; }

} // namespace PDCommon::Model
