#pragma once

#include <fstream>
#include <string>
#include <vector>

namespace GRPD {
namespace IO {

enum fileFormat { ascii, binary };
enum dataType { Int32, Int64, Float32, Float64 };

struct dataInfoStruct {
  dataType type;
  std::string typeStr;
  std::string name;
  void *ptr;
  int width;
};

class Writer {
protected:
  std::ofstream outFile;
  int pointsNumber;
  fileFormat format;
  dataInfoStruct pointsInfo;
  std::vector<dataInfoStruct> pointsVariable;

public:
  Writer();
  Writer(std::string filename, int nP, fileFormat f);
  virtual ~Writer();

  void open(std::string filename, fileFormat f);

  virtual bool setPointsNumber(int nP) = 0;
  int getPointsNumber();

  void setFormat(fileFormat f);
  fileFormat getFormat();

  void setPointsInfo(dataType type, void *ptr, std::string name = "Points",
                     int width = 3);
  void setPointsVariable(dataType type, void *ptr, std::string name, int width);
  virtual void write() = 0;
};

} // namespace IO
} // namespace GRPD
