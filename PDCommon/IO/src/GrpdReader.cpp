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
// trim: 鍘婚櫎瀛楃涓查灏剧┖鐧藉瓧绗︼紙绌烘牸銆佸埗琛ㄧ銆佸洖杞︺€佹崲琛岋級
// ---------------------------------------------------------------------------
std::string GrpdReader::trim(const std::string &s) {
  auto start = s.find_first_not_of(" \t\r\n");
  if (start == std::string::npos)
    return "";
  auto end = s.find_last_not_of(" \t\r\n");
  return s.substr(start, end - start + 1);
}

// ---------------------------------------------------------------------------
// tokenizeCsv: 灏嗛€楀彿鍒嗛殧鐨勪竴琛屾枃鏈垎鍓蹭负淇壀鍚庣殑瀛楁鏁扮粍
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
// scanFile: 閫氱敤鏂囦欢鎵弿鍣?
//   灏佽锛氭枃浠舵墦寮€ 鈫?閫愯璇诲彇 鈫?绌鸿/娉ㄩ噴璺宠繃 鈫?娈垫爣璁扮姸鎬佸垏鎹?
//   閬囧埌鏈夋晥鏁版嵁琛屾椂锛岃皟鐢?callback(state, tokens, lineNumber)
// ---------------------------------------------------------------------------
bool GrpdReader::scanFile(const std::string &filepath, LineCallback callback,
                          const std::string &logPrefix) {
  // 1. 鎵撳紑鏂囦欢
  std::ifstream file(filepath);
  if (!file.is_open()) {
    LOG_ERROR("[" + logPrefix + "] Error: cannot open file \"" + filepath +
              "\"");
    return false;
  }

  // 2. 鐘舵€佹満鍒濆鍖?
  ParseState state = ParseState::IDLE;
  std::string line;
  int lineNumber = 0;

  // 3. 閫愯鎵弿
  while (std::getline(file, line)) {
    lineNumber++;

    // 鍘婚櫎棣栧熬绌虹櫧
    std::string trimmedLine = trim(line);

    // 璺宠繃绌鸿
    if (trimmedLine.empty()) {
      continue;
    }

    // 璺宠繃娉ㄩ噴琛岋紙浠?# 寮€澶达級
    if (trimmedLine[0] == '#') {
      continue;
    }

    // ----- 娈垫爣璁版娴嬶細鍒囨崲鐘舵€?-----
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

    // IDLE 鐘舵€佷笅蹇界暐鏁版嵁琛?
    if (state == ParseState::IDLE) {
      continue;
    }

    // ----- 鍒嗗壊瀛楁骞惰皟鐢ㄥ洖璋?-----
    auto tokens = tokenizeCsv(line);
    if (!callback(state, tokens, lineNumber)) {
      break; // 鍥炶皟杩斿洖 false 鍒欑粓姝㈡壂鎻?
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
// read: 璇诲彇 *PARTICLE 娈碉紝濉厖绮掑瓙鍑犱綍鏁版嵁鍒?ParticleManager
//       *LOAD 娈典粎缁熻琛屾暟锛屼笉鍋氬疄闄呭鐞?
// ---------------------------------------------------------------------------
bool GrpdReader::read(const std::string &filepath,
                      PDCommon::Model::ParticleManager &manager) {
  int particleCount = 0;
  int loadCount = 0;

  bool ok = scanFile(
      filepath,
      [&](ParseState state, const std::vector<std::string> &tokens,
          int lineNumber) -> bool {
        // ===== *PARTICLE 娈碉細瑙ｆ瀽 ID, PartID, MatID, X, Y, Z, Volume =====
        if (state == ParseState::READING_PARTICLES) {
          if (tokens.size() != 7) {
            LOG_WARNING(
                "[GrpdReader] Line " + std::to_string(lineNumber) +
                " particle field count=" + std::to_string(tokens.size()) +
                ", expected 7, skipped");
            return true; // 璺宠繃褰撳墠琛岋紝缁х画鎵弿
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

        // ===== *LOAD 娈碉細浠呰鏁帮紝瀹為檯澶勭悊鐢?readLoads() 璐熻矗 =====
        if (state == ParseState::READING_LOADS) {
          loadCount++;
        }

        return true; // 缁х画鎵弿
      },
      "GrpdReader");

  if (!ok)
    return false;

  // 鎵撳嵃姹囨€荤粺璁?
  LOG_INFO("[GrpdReader] ============================================");
  LOG_INFO("[GrpdReader]    File reading complete!");
  LOG_INFO("[GrpdReader]    Total particles: " + std::to_string(particleCount));
  LOG_INFO("[GrpdReader]    Total load entries: " + std::to_string(loadCount));
  LOG_INFO("[GrpdReader] ============================================");

  return true;
}

// ---------------------------------------------------------------------------
// readLoads: 璇诲彇 *LOAD 娈碉紝灏嗚浇鑽烽€氳繃 FieldManager 鏂藉姞鍒扮墿鐞嗗満
//            浠呭鐞?*LOAD 娈碉紝蹇界暐 *PARTICLE 娈垫暟鎹?
// ---------------------------------------------------------------------------
bool GrpdReader::readLoads(const std::string &filepath,
                           PDCommon::Core::PDContext &simulater) {
  auto &fieldManager = simulater.getFieldManager();
  auto &bcManager = simulater.getBCManager();

  // 妫€鏌ユ俯搴﹀満鏄惁宸叉敞鍐?
  if (!fieldManager.hasField("Temperature")) {
    LOG_WARNING("[GrpdReader] Temperature field not registered. Skipping loads.");
    return false;
  }

  // 鑾峰彇娓╁害鍦烘寚閽堬紙鐢ㄤ簬 Dirichlet 鍒濆鍊艰缃級
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

          // 浣跨敤 BcID 浣滀负 BC 鍚嶇О鍓嶇紑锛屽疄鐜板垎绫绘爣璁?
          std::string bcName =
              type + "_BC" + std::to_string(bcId) + "_P" + std::to_string(id);

          // 閫氳繃 BCRegistry 宸ュ巶鍒涘缓 BC 瀹炰緥锛堟秷闄?if/else锛?
          auto bc = PDCommon::BC::BCRegistry::getInstance().createBC(type, bcName);
          if (!bc) {
            LOG_WARNING("[GrpdReader] Unknown BC type: " + type);
            otherCount++;
          } else {
            bc->initialize(fieldManager, id, values);

            // Dirichlet 绾︽潫闇€瑕佸悓姝ヨ缃垵濮嬫俯搴﹀€?
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
