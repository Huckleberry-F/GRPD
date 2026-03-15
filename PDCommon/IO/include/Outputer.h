#ifndef GRPD_IO_OUTPUTER_H
#define GRPD_IO_OUTPUTER_H

#include <cstddef>
#include <string>
#include <vector>

namespace GRPD::Field {
class FieldManager;
}

namespace GRPD::IO {

class Outputer {
public:
  void addScalarField(const std::string &fieldName);
  void addVectorField(const std::string &fieldName);
  void addIntField(const std::string &fieldName);
  void clear();

  bool writeVTK(const std::string &filename,
                const GRPD::Field::FieldManager &fm,
                size_t numParticles) const;

private:
  std::vector<std::string> scalarFields_;
  std::vector<std::string> vectorFields_;
  std::vector<std::string> intFields_;
};

} // namespace GRPD::IO

#endif // GRPD_IO_OUTPUTER_H
