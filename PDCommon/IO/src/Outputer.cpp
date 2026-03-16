#include "Outputer.h"

#include "FieldManager.h"
#include "Logger.h"
#include "TypedField.h"
#include "VtkWriter.h"
#include <algorithm>

namespace PDCommon::IO {

namespace {

void addUnique(std::vector<std::string> &fields, const std::string &fieldName) {
  if (std::find(fields.begin(), fields.end(), fieldName) == fields.end()) {
    fields.push_back(fieldName);
  }
}

bool validateFieldSize(const std::string &fieldName, size_t actualSize,
                       size_t expectedSize) {
  if (actualSize == expectedSize) {
    return true;
  }

  LOG_WARNING("[Outputer] Field '" + fieldName + "' size mismatch. Expected " +
              std::to_string(expectedSize) + ", got " +
              std::to_string(actualSize) + ". Skipping.");
  return false;
}

} // namespace

void Outputer::addScalarField(const std::string &fieldName) {
  addUnique(scalarFields_, fieldName);
}

void Outputer::addVectorField(const std::string &fieldName) {
  addUnique(vectorFields_, fieldName);
}

void Outputer::addIntField(const std::string &fieldName) {
  addUnique(intFields_, fieldName);
}

void Outputer::clear() {
  scalarFields_.clear();
  vectorFields_.clear();
  intFields_.clear();
}

void Outputer::setFormat(fileFormat fmt) { format_ = fmt; }

bool Outputer::writeVTK(const std::string &filename,
                        const PDCommon::Field::FieldManager &fm,
                        size_t numParticles) const {
  if (numParticles == 0) {
    LOG_WARNING("[Outputer] No particles available for VTK export.");
    return false;
  }

  const auto *coordsField = fm.getFieldAs<double>("Coords");
  if (!coordsField) {
    LOG_ERROR("[Outputer] Required field 'Coords' not found.");
    return false;
  }
  if (coordsField->getDim() != 3) {
    LOG_ERROR("[Outputer] Field 'Coords' must have dimension 3.");
    return false;
  }
  if (!validateFieldSize("Coords", coordsField->size(), numParticles)) {
    return false;
  }

  VtkWriter writer(filename, static_cast<int>(numParticles), format_);
  writer.setPointsInfo(Float64,
                       const_cast<double *>(coordsField->dataPtr()),
                       "Coords",
                       3);

  for (const auto &fieldName : intFields_) {
    const auto *field = fm.getFieldAs<int>(fieldName);
    if (!field) {
      LOG_WARNING("[Outputer] Integer field '" + fieldName +
                  "' not found. Skipping.");
      continue;
    }
    if (field->getDim() != 1) {
      LOG_WARNING("[Outputer] Integer field '" + fieldName +
                  "' must be scalar. Skipping.");
      continue;
    }
    if (!validateFieldSize(fieldName, field->size(), numParticles)) {
      continue;
    }

    writer.setPointsVariable(Int32,
                             const_cast<int *>(field->dataPtr()),
                             fieldName,
                             1);
  }

  for (const auto &fieldName : scalarFields_) {
    const auto *field = fm.getFieldAs<double>(fieldName);
    if (!field) {
      LOG_WARNING("[Outputer] Scalar field '" + fieldName +
                  "' not found. Skipping.");
      continue;
    }
    if (field->getDim() != 1) {
      LOG_WARNING("[Outputer] Scalar field '" + fieldName +
                  "' must have dimension 1. Skipping.");
      continue;
    }
    if (!validateFieldSize(fieldName, field->size(), numParticles)) {
      continue;
    }

    writer.setPointsVariable(Float64,
                             const_cast<double *>(field->dataPtr()),
                             fieldName,
                             1);
  }

  for (const auto &fieldName : vectorFields_) {
    const auto *field = fm.getFieldAs<double>(fieldName);
    if (!field) {
      LOG_WARNING("[Outputer] Vector field '" + fieldName +
                  "' not found. Skipping.");
      continue;
    }
    if (field->getDim() != 3) {
      LOG_WARNING("[Outputer] Vector field '" + fieldName +
                  "' must have dimension 3. Skipping.");
      continue;
    }
    if (!validateFieldSize(fieldName, field->size(), numParticles)) {
      continue;
    }

    writer.setPointsVariable(Float64,
                             const_cast<double *>(field->dataPtr()),
                             fieldName,
                             3);
  }

  writer.write();
  return true;
}

} // namespace PDCommon::IO
