#include "VtkWriter.h"
#include <iomanip>
#include <iostream>

namespace PDCommon {
namespace IO {

VtkWriter::VtkWriter() : Writer() {
  cells.ptr = nullptr;
  cells.name = "CELLS";
  cells.type = Int32;
  cells.typeStr = "Int32";
  cells.width = 2; // 物质点是2，FEM网格可能是5或者9

  cellTypes.ptr = nullptr;
  cellTypes.name = "CELL_TYPES";
  cellTypes.type = Int32;
  cellTypes.typeStr = "Int32";
  cellTypes.width = 1;

  buf = nullptr;
}

VtkWriter::VtkWriter(std::string filename, int nP, fileFormat f)
    : Writer(filename, nP, f) {
  cells.ptr = nullptr;
  cells.name = "CELLS";
  cells.type = Int32;
  cells.typeStr = "Int32";
  cells.width = 2; // 物质点是2，FEM网格可能是5或者9

  cellTypes.ptr = nullptr;
  cellTypes.name = "CELL_TYPES";
  cellTypes.type = Int32;
  cellTypes.typeStr = "Int32";
  cellTypes.width = 1;

  setPointsNumber(nP);
}

VtkWriter::~VtkWriter() {
  if (pointsNumber > 0) {
    delete[] static_cast<int *>(cells.ptr);
    delete[] static_cast<int *>(cellTypes.ptr);
    delete[] buf;
  }
}

bool VtkWriter::setPointsNumber(int nP) {
  try {
    if (nP < 0) {
      std::cout << "VtkWriter::setPointsNumber Failed: The number of points "
                   "must greater than zero!"
                << std::endl;
      throw std::runtime_error("");
    }

    if (nP == pointsNumber) {
      return true;
    } else {
      if (pointsNumber == -1) {

        pointsNumber = nP;
        cells.ptr = new int[nP * 2];
        cells.name = "CELLS";
        cells.type = Int32;
        cells.typeStr = "Int32";
        cells.width = 2; // 物质点是2，FEM网格可能是5或者9

        cellTypes.ptr = new int[nP];
        cellTypes.name = "CELL_TYPES";
        cellTypes.type = Int32;
        cellTypes.typeStr = "Int32";
        cellTypes.width = 1;

        buf = new char[nP * 9 * 8];
      } else {
        delete[] static_cast<int *>(cells.ptr);
        delete[] static_cast<int *>(cellTypes.ptr);
        delete[] buf;

        pointsNumber = nP;
        cells.ptr = new int[nP * 2];
        cellTypes.ptr = new int[nP];
        buf = new char[nP * 9 * 8];
      }
      int *cellsPTR = static_cast<int *>(cells.ptr);
      int *cellTypesPTR = static_cast<int *>(cellTypes.ptr);
      for (int i = 0; i < nP; i++) {
        int idx = 2 * i;
        cellsPTR[idx] = 1;
        cellsPTR[idx + 1] = i;
        cellTypesPTR[i] = 1;
      }
      return true;
    }
  } catch (const std::exception &e) {
    std::cerr << "Error:" << e.what();
    return false;
  }
}

void VtkWriter::write() {
  outFile << "# vtk DataFile Version 4.2";
  outFile << '\n';
  outFile << "vtk output";
  outFile << '\n';
  std::string Format;
  switch (format) {
  case ascii:
    Format = "ASCII";
    break;
  case binary:
    Format = "BINARY";
    break;
  }
  outFile << Format << std::endl;
  outFile << "DATASET UNSTRUCTURED_GRID";
  outFile << '\n';
  writePoints();
  writeCells();
  writeCellTypes();
  writePointsVariable();
}

void VtkWriter::writePoints() {
  try {
    switch (pointsInfo.type) {
    case Float32:
      outFile << "POINTS " << pointsNumber << " " << "float";
      outFile << '\n';
      switch (format) {
      case ascii:
        writeAscii(static_cast<float *>(pointsInfo.ptr), pointsInfo.width);
        break;
      case binary:
        convertEndian(buf, pointsInfo.ptr, Float32,
                      pointsNumber * pointsInfo.width);
        writeBinary(reinterpret_cast<float *>(buf), pointsInfo.width);
        break;
      }
      break;
    case Float64:
      outFile << "POINTS " << pointsNumber << " " << "double";
      outFile << '\n';
      switch (format) {
      case ascii:
        writeAscii(static_cast<double *>(pointsInfo.ptr), pointsInfo.width);
        break;
      case binary:
        convertEndian(buf, pointsInfo.ptr, Float64,
                      pointsNumber * pointsInfo.width);
        writeBinary(reinterpret_cast<double *>(buf), pointsInfo.width);
        break;
      }
      break;
    default:
      std::cout << "VtkWriter::writePoints ERROR: Points only can be written "
                   "as double/float form!\n"
                << std::endl;
      throw std::runtime_error("");
    }
  } catch (const std::exception &e) {
    std::cerr << "Error:" << e.what();
  }
}

void VtkWriter::writeCells() {
  outFile << "CELLS " << pointsNumber << " " << pointsNumber * 2;
  outFile << '\n';
  switch (format) {
  case ascii:
    writeAscii(static_cast<int *>(cells.ptr), cells.width);
    break;
  case binary:
    convertEndian(buf, cells.ptr, Int32, pointsNumber * cells.width);
    writeBinary(reinterpret_cast<int *>(buf), cells.width);
    break;
  }
}

void VtkWriter::writeCellTypes() {
  outFile << "CELL_TYPES " << pointsNumber;
  outFile << '\n';
  switch (format) {
  case ascii:
    writeAscii(static_cast<int *>(cellTypes.ptr), cellTypes.width);
    break;
  case binary:
    convertEndian(buf, cellTypes.ptr, Int32, pointsNumber * cellTypes.width);
    writeBinary(reinterpret_cast<int *>(buf), cellTypes.width);
    break;
  }
}

void VtkWriter::writePointsVariable() {
  outFile << "POINT_DATA " << pointsNumber;
  outFile << '\n';
  outFile << "FIELD FieldData " << pointsVariable.size();
  outFile << '\n';
  for (auto i = pointsVariable.begin(); i != pointsVariable.end(); i++) {
    outFile << i->name << " " << i->width << " " << pointsNumber << " ";
    switch (format) {
    case ascii:
      switch (i->type) {
      case Int32:
        outFile << "int";
        outFile << '\n';
        writeAscii(static_cast<int *>(i->ptr), i->width);
        break;
      case Int64:
        outFile << "long";
        outFile << '\n';
        writeAscii(static_cast<int *>(i->ptr), i->width);
        break;
      case Float32:
        outFile << "float";
        outFile << '\n';
        writeAscii(static_cast<float *>(i->ptr), i->width);
        break;
      case Float64:
        outFile << "double";
        outFile << '\n';
        writeAscii(static_cast<double *>(i->ptr), i->width);
        break;
      }
      break;
    case binary:
      switch (i->type) {
      case Int32:
        outFile << "int";
        outFile << '\n';
        convertEndian(buf, i->ptr, Int32, pointsNumber * i->width);
        writeBinary(reinterpret_cast<int *>(buf), i->width);
        break;
      case Int64:
        outFile << "long";
        outFile << '\n';
        convertEndian(buf, i->ptr, Int64, pointsNumber * i->width);
        writeBinary(reinterpret_cast<long *>(buf), i->width);
        break;
      case Float32:
        outFile << "float";
        outFile << '\n';
        convertEndian(buf, i->ptr, Float32, pointsNumber * i->width);
        writeBinary(reinterpret_cast<float *>(buf), i->width);
        break;
      case Float64:
        outFile << "double";
        outFile << '\n';
        convertEndian(buf, i->ptr, Float64, pointsNumber * i->width);
        writeBinary(reinterpret_cast<double *>(buf), i->width);
        break;
      }
      break;
    }
  }
}

template <typename T> void VtkWriter::writeAscii(T *arr, int width) {
  std::ios_base::fmtflags originalFlags = outFile.flags();
  std::streamsize originalPrecision = outFile.precision();
  outFile << std::scientific;
  outFile << std::setprecision(8);
  for (int i = 0; i < pointsNumber; ++i) {
    auto *rowStart = arr + i * width;
    for (int j = 0; j < width; j++) {
      T value = rowStart[j];
      outFile << std::setw(16) << value;
      if (j < width - 1) {
        outFile << " ";
      }
    }
    outFile << '\n';
  }
  outFile.flags(originalFlags);
  outFile.precision(originalPrecision);
}

template <typename T> void VtkWriter::writeBinary(T *arr, int width) {
  try {
    size_t sz = static_cast<unsigned long>(pointsNumber * width) * sizeof(T);
    if (!outFile.write(reinterpret_cast<char *>(arr), static_cast<long>(sz))) {
      throw std::runtime_error("Failed to write data in binary form!");
    }
  } catch (const std::exception &e) {
    std::cerr << "Error:" << e.what();
  }
}

void VtkWriter::convertEndian(char *buf, void *ptr, dataType type, int length) {
  char *cptr = static_cast<char *>(ptr);
  int width = 0;
  switch (type) {
  case Int32:
  case Float32:
    width = 4;
    break;
  case Int64:
  case Float64:
    width = 8;
    break;
  default:
    return;
  }
  for (int i = 0; i < length; i++) {
    char *src = cptr + i * width;
    char *dst = buf + i * width;
    for (int j = 0; j < width; j++) {
      dst[j] = src[width - 1 - j];
    }
  }
}

} // namespace IO
} // namespace PDCommon
