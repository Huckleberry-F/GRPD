#include "MeshData.h"
#include "FieldManager.h"
#include "Logger.h"
#include "ParticleManager.h"
#include "TypedField.h"

namespace PDCommon::IO {

namespace {
// 字段名常量（与原 GrpdReader 保持一致）
constexpr const char *kIdFieldName = "ID";
constexpr const char *kPartIdFieldName = "PartID";
constexpr const char *kMatIdFieldName = "MatID";
constexpr const char *kCoordsFieldName = "Coords";
constexpr const char *kVolumeFieldName = "Volume";
} // namespace

// ---------------------------------------------------------------------------
// ensureParticleFields: 注册粒子基础几何场
// ---------------------------------------------------------------------------
void MeshData::ensureParticleFields(PDCommon::Field::FieldManager &fm) {
  fm.registerField<int>(kIdFieldName, 1);
  fm.registerField<int>(kPartIdFieldName, 1);
  fm.registerField<int>(kMatIdFieldName, 1);
  fm.registerField<double>(kCoordsFieldName, 3);
  fm.registerField<double>(kVolumeFieldName, 1);
}

// ---------------------------------------------------------------------------
// populateParticleFields: 从 ParticleManager 填充 FieldManager
// ---------------------------------------------------------------------------
bool MeshData::populateParticleFields(
    const PDCommon::Model::ParticleManager &pm,
    PDCommon::Field::FieldManager &fm) {
  auto *idField = fm.getFieldAs<int>(kIdFieldName);
  auto *partIdField = fm.getFieldAs<int>(kPartIdFieldName);
  auto *matIdField = fm.getFieldAs<int>(kMatIdFieldName);
  auto *coordsField = fm.getFieldAs<double>(kCoordsFieldName);
  auto *volumeField = fm.getFieldAs<double>(kVolumeFieldName);

  if (!idField || !partIdField || !matIdField || !coordsField || !volumeField) {
    LOG_ERROR(
        "[MeshData] Particle fields are not fully registered in FieldManager.");
    return false;
  }

  const size_t numParticles = pm.getTotalParticles();
  if (idField->size() != numParticles || partIdField->size() != numParticles ||
      matIdField->size() != numParticles ||
      coordsField->size() != numParticles ||
      volumeField->size() != numParticles) {
    LOG_ERROR("[MeshData] Particle field size mismatch. Ensure resizeAll() "
              "has been called before populateParticleFields().");
    return false;
  }

  for (size_t i = 0; i < numParticles; ++i) {
    const auto &particle = pm.getParticle(static_cast<int>(i));
    const auto &coords = particle.getCoords();

    idField->set(static_cast<int>(i), particle.getId());
    partIdField->set(static_cast<int>(i), particle.getPartId());
    matIdField->set(static_cast<int>(i), particle.getMatId());
    coordsField->set(static_cast<int>(i), coords[0], 0);
    coordsField->set(static_cast<int>(i), coords[1], 1);
    coordsField->set(static_cast<int>(i), coords[2], 2);
    volumeField->set(static_cast<int>(i), particle.getVolume());
  }

  LOG_INFO("[MeshData] Populated particle geometry fields into FieldManager "
           "for " +
           std::to_string(numParticles) + " particles.");
  return true;
}

} // namespace PDCommon::IO
