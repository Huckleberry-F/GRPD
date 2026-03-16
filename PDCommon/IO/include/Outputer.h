#ifndef PDCOMMON_IO_OUTPUTER_H
#define PDCOMMON_IO_OUTPUTER_H

#include "Writer.h"
#include <cstddef>
#include <string>
#include <vector>

namespace PDCommon::Field {
class FieldManager;
}

namespace PDCommon::IO {

class Outputer {
public:
  void addScalarField(const std::string &fieldName);
  void addVectorField(const std::string &fieldName);
  void addIntField(const std::string &fieldName);
  void clear();

  /// @brief 设置输出格式 (ascii 或 binary)，默认为 ascii
  void setFormat(fileFormat fmt);

  bool writeVTK(const std::string &filename,
                const PDCommon::Field::FieldManager &fm,
                size_t numParticles) const;

private:
  fileFormat format_ = ascii;  // 输出格式，默认 ASCII
  std::vector<std::string> scalarFields_;
  std::vector<std::string> vectorFields_;
  std::vector<std::string> intFields_;
};

} // namespace PDCommon::IO

#endif // PDCOMMON_IO_OUTPUTER_H
