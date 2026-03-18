// ============================================================================
// GrpdReader.cpp - High-speed state-machine parser for .grpd files
// Responsibility: Scan .grpd file line by line, switch parse state on section
//                 markers, fill particle data into ParticleManager, and apply
//                 load data to physical field managers.
// ============================================================================

#include "GrpdReader.h"
#include "BCManager.h"
#include "BCRegistry.h"
#include "TypedField.h"
#include "FieldManager.h"
#include "Logger.h"
#include "PDContext.h"
#include "ParticleManager.h"
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace PDCommon::IO {

namespace {

constexpr const char *kIdFieldName = "ID";
constexpr const char *kPartIdFieldName = "PartID";
constexpr const char *kMatIdFieldName = "MatID";
constexpr const char *kCoordsFieldName = "Coords";
constexpr const char *kVolumeFieldName = "Volume";

} // namespace

// ---------------------------------------------------------------------------
// trim: 去除字符串首尾空白字符（空格、制表符、回车、换行）
// ---------------------------------------------------------------------------
std::string GrpdReader::trim(const std::string &s) {
  auto start = s.find_first_not_of(" \t\r\n");
  if (start == std::string::npos)
    return "";
  auto end = s.find_last_not_of(" \t\r\n");
  return s.substr(start, end - start + 1);
}

// ---------------------------------------------------------------------------
// tokenizeCsv: 将逗号分隔的一行文本分割为修剪后的字段数组
// ---------------------------------------------------------------------------
std::vector<std::string> GrpdReader::tokenizeCsv(const std::string &line) {
  std::vector<std::string> tokens;
  std::istringstream ss(line);
  std::string token;
  while (std::getline(ss, token, ',')) {
    tokens.push_back(trim(token));
  }
  return tokens;
}

// ---------------------------------------------------------------------------
// scanFile: 通用文件扫描器
//   封装：文件打开 → 逐行读取 → 空行/注释跳过 → 段标记状态切换
//   遇到有效数据行时，调用 callback(state, tokens, lineNumber)
// ---------------------------------------------------------------------------
bool GrpdReader::scanFile(const std::string &filepath, LineCallback callback,
                          const std::string &logPrefix) {
  // 1. 打开文件
  std::ifstream file(filepath);
  if (!file.is_open()) {
    LOG_ERROR("[" + logPrefix + "] Error: cannot open file \"" + filepath +
              "\"");
    return false;
  }

  // 2. 状态机初始化
  ParseState state = ParseState::IDLE;
  std::string line;
  int lineNumber = 0;

  // 3. 逐行扫描
  while (std::getline(file, line)) {
    lineNumber++;

    // 去除首尾空白
    std::string trimmedLine = trim(line);

    // 跳过空行
    if (trimmedLine.empty()) {
      continue;
    }

    // 跳过注释行（以 # 开头）
    if (trimmedLine[0] == '#') {
      continue;
    }

    // ----- 段标记检测：切换状态 -----
    if (trimmedLine == "*PARTICLE") {
      state = ParseState::READING_PARTICLES;
      LOG_INFO("[" + logPrefix + "] Entering *PARTICLE section (Line " +
               std::to_string(lineNumber) + ")");
      continue;
    }

    if (trimmedLine == "*LOAD") {
      state = ParseState::READING_LOADS;
      LOG_INFO("[" + logPrefix + "] Entering *LOAD section (Line " +
               std::to_string(lineNumber) + ")");
      continue;
    }

    // IDLE 状态下忽略数据行
    if (state == ParseState::IDLE) {
      continue;
    }

    // ----- 分割字段并调用回调 -----
    auto tokens = tokenizeCsv(line);
    if (!callback(state, tokens, lineNumber)) {
      break; // 回调返回 false 则终止扫描
    }
  }

  file.close();
  return true;
}

void GrpdReader::ensureParticleFields(PDCommon::Field::FieldManager &fm) {
  fm.registerField<int>(kIdFieldName, 1);
  fm.registerField<int>(kPartIdFieldName, 1);
  fm.registerField<int>(kMatIdFieldName, 1);
  fm.registerField<double>(kCoordsFieldName, 3);
  fm.registerField<double>(kVolumeFieldName, 1);
}

bool GrpdReader::populateParticleFields(const PDCommon::Model::ParticleManager &pm,
                                        PDCommon::Field::FieldManager &fm) {
  auto *idField = fm.getFieldAs<int>(kIdFieldName);
  auto *partIdField = fm.getFieldAs<int>(kPartIdFieldName);
  auto *matIdField = fm.getFieldAs<int>(kMatIdFieldName);
  auto *coordsField = fm.getFieldAs<double>(kCoordsFieldName);
  auto *volumeField = fm.getFieldAs<double>(kVolumeFieldName);

  if (!idField || !partIdField || !matIdField || !coordsField ||
      !volumeField) {
    LOG_ERROR("[GrpdReader] Particle fields are not fully registered in "
              "FieldManager.");
    return false;
  }

  const size_t numParticles = pm.getTotalParticles();
  if (idField->size() != numParticles || partIdField->size() != numParticles ||
      matIdField->size() != numParticles ||
      coordsField->size() != numParticles ||
      volumeField->size() != numParticles) {
    LOG_ERROR("[GrpdReader] Particle field size mismatch. Ensure resizeAll() "
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

  LOG_INFO("[GrpdReader] Populated particle geometry fields into FieldManager "
           "for " + std::to_string(numParticles) + " particles.");
  return true;
}

// ---------------------------------------------------------------------------
// read: 读取 *PARTICLE 段，填充粒子几何数据到 ParticleManager
//       *LOAD 段仅统计行数，不做实际处理
// ---------------------------------------------------------------------------
bool GrpdReader::read(const std::string &filepath,
                      PDCommon::Model::ParticleManager &manager) {
  int particleCount = 0;
  int loadCount = 0;

  bool ok = scanFile(
      filepath,
      [&](ParseState state, const std::vector<std::string> &tokens,
          int lineNumber) -> bool {
        // ===== *PARTICLE 段：解析 ID, PartID, MatID, X, Y, Z, Volume =====
        if (state == ParseState::READING_PARTICLES) {
          if (tokens.size() != 7) {
            LOG_WARNING(
                "[GrpdReader] Line " + std::to_string(lineNumber) +
                " particle field count=" + std::to_string(tokens.size()) +
                ", expected 7, skipped");
            return true; // 跳过当前行，继续扫描
          }

          try {
            int id = std::stoi(tokens[0]);
            int partId = std::stoi(tokens[1]);
            int matId = std::stoi(tokens[2]);
            double x = std::stod(tokens[3]);
            double y = std::stod(tokens[4]);
            double z = std::stod(tokens[5]);
            double vol = std::stod(tokens[6]);

            manager.addParticle(id, partId, matId, x, y, z, vol);
            particleCount++;
          } catch (const std::exception &e) {
            LOG_ERROR("[GrpdReader] Line " + std::to_string(lineNumber) +
                      " parse failed: " + std::string(e.what()));
          }
        }

        // ===== *LOAD 段：仅计数，实际处理由 readLoads() 负责 =====
        if (state == ParseState::READING_LOADS) {
          loadCount++;
        }

        return true; // 继续扫描
      },
      "GrpdReader");

  if (!ok)
    return false;

  // 打印汇总统计
  LOG_INFO("[GrpdReader] ============================================");
  LOG_INFO("[GrpdReader]    File reading complete!");
  LOG_INFO("[GrpdReader]    Total particles: " + std::to_string(particleCount));
  LOG_INFO("[GrpdReader]    Total load entries: " + std::to_string(loadCount));
  LOG_INFO("[GrpdReader] ============================================");

  return true;
}

// ---------------------------------------------------------------------------
// readLoads: 读取 *LOAD 段，将载荷通过 FieldManager 施加到物理场
//            仅处理 *LOAD 段，忽略 *PARTICLE 段数据
// ---------------------------------------------------------------------------
bool GrpdReader::readLoads(const std::string &filepath,
                           PDCommon::Core::PDContext &simulater) {
  auto &fieldManager = simulater.getFieldManager();
  auto &bcManager = simulater.getBCManager();

  // 检查温度场是否已注册
  if (!fieldManager.hasField("Temperature")) {
    LOG_WARNING("[GrpdReader] Temperature field not registered. Skipping loads.");
    return false;
  }

  // 获取温度场指针（用于 Dirichlet 初始值设置）
  auto *tempField = fieldManager.getFieldAs<double>("Temperature");

  int tempCount = 0;
  int fluxCount = 0;
  int convCount = 0;
  int otherCount = 0;

  bool ok = scanFile(
      filepath,
      [&](ParseState state, const std::vector<std::string> &tokens,
          int lineNumber) -> bool {
        if (state != ParseState::READING_LOADS)
          return true;
        if (tokens.size() < 4)
          return true;

        try {
          int id = std::stoi(tokens[0]);
          int bcId = std::stoi(tokens[1]);
          std::string type = tokens[2];
          std::vector<double> values;
          for (size_t i = 3; i < tokens.size(); i++) {
            values.push_back(std::stod(tokens[i]));
          }

          // 使用 BcID 作为 BC 名称前缀，实现分类标记
          std::string bcName =
              type + "_BC" + std::to_string(bcId) + "_P" + std::to_string(id);

          // 通过 BCRegistry 工厂创建 BC 实例（消除 if/else）
          auto bc = PDCommon::BC::BCRegistry::getInstance().createBC(type, bcName);
          if (!bc) {
            LOG_WARNING("[GrpdReader] Unknown BC type: " + type);
            otherCount++;
          } else {
            bc->initialize(fieldManager, id, values);

            // Dirichlet 约束需要同步设置初始温度值
            if (bc->isConstraint()) {
              tempField->set(id, values[0]);
              tempCount++;
            } else if (type == "FLUX") {
              auto *heatFluxField = fieldManager.getFieldAs<double>("HeatFlux");
              if (heatFluxField) {
                heatFluxField->set(id, values[0]);
              }
              fluxCount++;
            } else {
              convCount++;
            }

            bcManager.addBC(std::move(bc));
          }
        } catch (const std::exception &e) {
          LOG_ERROR("[GrpdReader::readLoads] Line " +
                    std::to_string(lineNumber) +
                    " parse failed: " + std::string(e.what()));
        }

        return true;
      },
      "GrpdReader::readLoads");

  if (!ok)
    return false;

  LOG_INFO("[GrpdReader::readLoads] Load statistics:");
  LOG_INFO("  - Temperature (Dirichlet): " + std::to_string(tempCount));
  LOG_INFO("  - Heat Flux (Neumann):    " + std::to_string(fluxCount));
  LOG_INFO("  - Convection (Robin):     " + std::to_string(convCount));
  if (otherCount > 0)
    LOG_WARNING("  - Unknown types: " + std::to_string(otherCount));

  return true;
}

} // namespace PDCommon::IO
