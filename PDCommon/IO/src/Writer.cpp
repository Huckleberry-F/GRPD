#include "Writer.h"
#include <iostream>

namespace GRPD {
namespace IO {

Writer::Writer() {
  pointsNumber = -1;
  format = ascii;
  pointsInfo.ptr = nullptr;
}

Writer::Writer(std::string filename, int nP, fileFormat f) {
  if (f == ascii) {
    outFile.open(filename, std::ios::out);
  } else {
    outFile.open(filename, std::ios::binary | std::ios::out);
  }
  pointsNumber = -1; // Force setPointsNumber to allocate in derived classes
  format = f;
  pointsInfo.ptr = nullptr;
}

Writer::~Writer() {
  if (outFile.is_open()) {
    outFile.close();
  }
}

void Writer::open(std::string filename, fileFormat f) {
  if (outFile.is_open()) {
    outFile.close();
  }
  format = f;
  if (f == ascii) {
    try {
      outFile.open(filename, std::ios::out);
      if (!outFile.is_open()) {
        std::cout << "open file Error:" << filename << "does not exist!"
                  << std::endl;
        throw std::runtime_error("open file Error:" + std::string(filename));
      }
    } catch (const std::exception &e) {
      std::cerr << "Error:" << e.what();
    }
  } else {
    try {
      outFile.open(filename, std::ios::binary | std::ios::out);
      if (!outFile.is_open()) {
        std::cout << "open file Error:" << filename << "does not exist!"
                  << std::endl;
        throw std::runtime_error("open file Error:" + std::string(filename));
      }
    } catch (const std::exception &e) {
      std::cerr << "Error:" << e.what();
    }
  }
}

bool Writer::setPointsNumber(int nP) {
  pointsNumber = nP;
  return true;
}

int Writer::getPointsNumber() { return pointsNumber; }

void Writer::setFormat(fileFormat f) { format = f; }

fileFormat Writer::getFormat() { return format; }

void Writer::setPointsInfo(dataType type, void *ptr, std::string name,
                           int width) {
  pointsInfo.type = type;
  switch (type) {
  case Int32:
    pointsInfo.typeStr = "Int32";
    break;
  case Int64:
    pointsInfo.typeStr = "Int64";
    break;
  case Float32:
    pointsInfo.typeStr = "Float32";
    break;
  case Float64:
    pointsInfo.typeStr = "Float64";
    break;
  }
  pointsInfo.name = name;
  pointsInfo.ptr = ptr;
  pointsInfo.width = 3;
}

void Writer::setPointsVariable(dataType type, void *ptr, std::string name,
                               int width) {
  dataInfoStruct dataInfo;
  dataInfo.type = type;
  switch (type) {
  case Int32:
    dataInfo.typeStr = "Int32";
    break;
  case Int64:
    dataInfo.typeStr = "Int64";
    break;
  case Float32:
    dataInfo.typeStr = "Float32";
    break;
  case Float64:
    dataInfo.typeStr = "Float64";
    break;
  }
  dataInfo.name = name;
  dataInfo.ptr = ptr;
  dataInfo.width = width;
  pointsVariable.push_back(dataInfo);
}

} // namespace IO
} // namespace GRPD
