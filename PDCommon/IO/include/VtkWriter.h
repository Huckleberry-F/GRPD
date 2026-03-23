#pragma once

#include "Writer.h"

namespace PDCommon {
namespace IO {

class VtkWriter : public Writer {
private:
  dataInfoStruct cells, cellTypes;
  char *buf;

public:
  VtkWriter();
  VtkWriter(std::string filename, int nP, fileFormat f);
  ~VtkWriter() override;

public:
  void write() override;
  bool setPointsNumber(int nP) override;

private:
  void writePoints();
  void writeCells();
  void writeCellTypes();
  void writePointsVariable();

  template <typename T> void writeAscii(T *arr, int width);

  template <typename T> void writeBinary(T *arr, int width);

  template <typename T> void writeArrayBinary(T *arr, int width);

  void convertEndian(char *buf, void *ptr, dataType type, int length);
};

} // namespace IO
} // namespace PDCommon
