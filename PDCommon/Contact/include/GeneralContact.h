#ifndef PDCOMMON_CONTACT_GENERALCONTACT_H
#define PDCOMMON_CONTACT_GENERALCONTACT_H

#include "IContactAlgorithm.h"
#include "IContactEvaluator.h"
#include "IContactForceLaw.h"
#include "IFrictionLaw.h"
#include "ContactSpatialGrid.h"
#include <memory>

namespace PDCommon::Contact {

class GeneralContact : public IContactAlgorithm {
public:
  explicit GeneralContact(const std::string &name,
                          std::unique_ptr<IContactEvaluator> evaluator,
                          std::unique_ptr<IContactForceLaw> normalLaw,
                          std::unique_ptr<IFrictionLaw> frictionLaw);
                          
  ~GeneralContact() override = default;

  std::string getTypeName() const override { return "General"; }
  void initialize(const YAML::Node &configNode) override;
  void computeContactForce(PDCommon::Core::PDContext &ctx) override;
  
  void setMassScaleFactor(double sf) override;

private:
  ContactSpatialGrid searchEngine_; // Axis A
  std::unique_ptr<IContactEvaluator> evaluator_; // Axis B
  std::unique_ptr<IContactForceLaw> normalLaw_; // Axis C
  std::unique_ptr<IFrictionLaw> frictionLaw_; // Axis D
};

} // namespace PDCommon::Contact

#endif // PDCOMMON_CONTACT_GENERALCONTACT_H
