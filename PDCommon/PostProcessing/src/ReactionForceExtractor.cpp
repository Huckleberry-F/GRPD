#include "ReactionForceExtractor.h"
#include "PostProcessorRegistry.h"
#include "PDContext.h"
#include "FieldManager.h"
#include "FieldRegistry.h"
#include "ParticleManager.h"
#include "MechanicalMaterial.h"
#include "Logger.h"

namespace PDCommon::PostProcessing {

void ReactionForceExtractor::initialize(PDCommon::Core::PDContext &ctx,
                                        const YAML::Node &config) {
  if (config["Name"]) {
    name_ = config["Name"].as<std::string>();
  }

  // Ensure ReactionForce field exists
  auto &fm = ctx.getFieldManager();
  if (!fm.hasField("ReactionForce")) {
    size_t numParticles = ctx.getParticleManager().getTotalParticles();
    auto rfField = PDCommon::Field::FieldRegistry::getInstance().createField(
        "DoubleField", "ReactionForce", 3);
    fm.addField(std::move(rfField));
    fm.getFieldAs<double>("ReactionForce")->resize(numParticles);
    LOG_INFO("[ReactionForceExtractor] Initialized physical field 'ReactionForce'.");
  }
}

void ReactionForceExtractor::preBCEvaluate(PDCommon::Core::PDContext &ctx,
                                           double currentTime, int stepId) {
  auto *accField = ctx.getFieldManager().getFieldAs<double>("Acceleration");
  auto *rfField = ctx.getFieldManager().getFieldAs<double>("ReactionForce");
  auto *volField = ctx.getFieldManager().getFieldAs<double>("Volume");

  if (!accField || !rfField) return;

  const double *acc = accField->dataPtr();
  double *rf = rfField->dataPtr();
  const double *vols = volField ? volField->dataPtr() : nullptr;

  auto &pm = ctx.getParticleManager();
  const auto &particles = pm.getAllParticles();
  int numParticles = static_cast<int>(particles.size());
  double massScale = ctx.getMassScaleFactor();

#pragma omp parallel for schedule(static)
  for (int i = 0; i < numParticles; ++i) {
    double vol = vols ? vols[i] : 1.0;
    double rho = 1.0;
    auto *mat = dynamic_cast<PDCommon::Material::MechanicalMaterial *>(
        particles[i].getMaterial());
    if (mat) {
      rho = mat->getDensity();
    }
    double mass = rho * vol * massScale;

    rf[i * 3 + 0] = acc[i * 3 + 0] * mass;
    rf[i * 3 + 1] = acc[i * 3 + 1] * mass;
    rf[i * 3 + 2] = acc[i * 3 + 2] * mass;
  }
}

} // namespace PDCommon::PostProcessing

REGISTER_POSTPROCESSOR(ReactionForce, PDCommon::PostProcessing::ReactionForceExtractor)
