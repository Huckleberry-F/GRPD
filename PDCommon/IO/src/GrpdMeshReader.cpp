#include "GrpdMeshReader.h"
#include "Logger.h"
#include "ReaderRegistry.h"
#include <fstream>
#include <sstream>

namespace PDCommon::IO {

// ---------------------------------------------------------------------------
// 构造函数
// ---------------------------------------------------------------------------
GrpdMeshReader::GrpdMeshReader(const std::string &name) : MeshReader(name) {}

// ---------------------------------------------------------------------------
// trim: 去除字符串首尾空白字符
// ---------------------------------------------------------------------------
std::string GrpdMeshReader::trim(const std::string &s) {
  auto start = s.find_first_not_of(" \t\r\n");
  if (start == std::string::npos)
    return "";
  auto end = s.find_last_not_of(" \t\r\n");
  return s.substr(start, end - start + 1);
}

// ---------------------------------------------------------------------------
// tokenizeCsv: 将逗号分隔的一行文本分割为修剪后的字段数组
// ---------------------------------------------------------------------------
std::vector<std::string>
GrpdMeshReader::tokenizeCsv(const std::string &line) {
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
bool GrpdMeshReader::scanFile(const std::string &filepath,
                              LineCallback callback,
                              const std::string &logPrefix) {
  std::ifstream file(filepath);
  if (!file.is_open()) {
    LOG_ERROR("[" + logPrefix + "] Error: cannot open file \"" + filepath +
              "\"");
    return false;
  }

  ParseState state = ParseState::IDLE;
  std::string line;
  int lineNumber = 0;

  while (std::getline(file, line)) {
    lineNumber++;
    std::string trimmedLine = trim(line);

    // 跳过空行和注释行
    if (trimmedLine.empty() || trimmedLine[0] == '#')
      continue;

    // 段标记检测：切换状态
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
    if (state == ParseState::IDLE)
      continue;

    // 分割字段并调用回调
    auto tokens = tokenizeCsv(line);
    if (!callback(state, tokens, lineNumber))
      break;
  }

  file.close();
  return true;
}

// ---------------------------------------------------------------------------
// read: 一次扫描同时解析 *PARTICLE 和 *LOAD 段
//   - *PARTICLE → meshData_ 的几何字段 (coords, nodeIDs, partIDs, matIDs, volumes)
//   - *LOAD     → meshData_.loads
// ---------------------------------------------------------------------------
bool GrpdMeshReader::read(const std::string &filepath) {
  LOG_INFO("[GrpdMeshReader] Reading file: " + filepath);
  meshData_.clear();

  int particleCount = 0;
  int loadCount = 0;

  bool ok = scanFile(
      filepath,
      [&](ParseState state, const std::vector<std::string> &tokens,
          int lineNumber) -> bool {
        // ===== *PARTICLE 段：解析 ID, PartID, MatID, X, Y, Z, Volume =====
        if (state == ParseState::READING_PARTICLES) {
          if (tokens.size() != 7) {
            LOG_WARNING("[GrpdMeshReader] Line " +
                        std::to_string(lineNumber) +
                        " field count=" + std::to_string(tokens.size()) +
                        ", expected 7, skipped");
            return true;
          }

          try {
            int id = std::stoi(tokens[0]);
            int partId = std::stoi(tokens[1]);
            int matId = std::stoi(tokens[2]);
            double x = std::stod(tokens[3]);
            double y = std::stod(tokens[4]);
            double z = std::stod(tokens[5]);
            double vol = std::stod(tokens[6]);

            meshData_.nodeIDs.push_back(id);
            meshData_.partIDs.push_back(partId);
            meshData_.matIDs.push_back(matId);
            meshData_.coords.push_back(x);
            meshData_.coords.push_back(y);
            meshData_.coords.push_back(z);
            meshData_.volumes.push_back(vol);
            particleCount++;
          } catch (const std::exception &e) {
            LOG_ERROR("[GrpdMeshReader] Line " +
                      std::to_string(lineNumber) +
                      " parse failed: " + std::string(e.what()));
          }
        }

        // ===== *LOAD 段：解析载荷条目 → meshData_.loads =====
        if (state == ParseState::READING_LOADS) {
          if (tokens.size() < 5)
            return true;

          try {
            LoadEntry entry;
            entry.nodeID = std::stoi(tokens[0]);
            entry.step = std::stoi(tokens[1]);
            entry.bcID = std::stoi(tokens[2]);
            entry.type = tokens[3];
            for (size_t i = 4; i < tokens.size(); ++i) {
                // 如果包含 %Table.txt% ，提取为 TableName
                if (tokens[i].size() >= 2 && tokens[i].front() == '%' && tokens[i].back() == '%') {
                    std::string tableName = tokens[i].substr(1, tokens[i].size() - 2);
                    entry.tableNames.push_back(tableName);
                    entry.values.push_back(1.0); // 被 Table 覆盖，提供 1.0 的放缩基准
                } else {
                    entry.tableNames.push_back("None");
                    entry.values.push_back(std::stod(tokens[i]));
                }
            }
            meshData_.loads.push_back(entry);
            loadCount++;
          } catch (const std::exception &e) {
            LOG_ERROR("[GrpdMeshReader] Line " +
                      std::to_string(lineNumber) +
                      " load parse failed: " + std::string(e.what()));
          }
        }

        return true;
      },
      "GrpdMeshReader");

  if (!ok)
    return false;

  LOG_INFO("[GrpdMeshReader] ============================================");
  LOG_INFO("[GrpdMeshReader]    File reading complete!");
  LOG_INFO("[GrpdMeshReader]    Total particles: " +
           std::to_string(particleCount));
  LOG_INFO("[GrpdMeshReader]    Total load entries: " +
           std::to_string(loadCount));
  LOG_INFO("[GrpdMeshReader] ============================================");

  return true;
}

// 编译期自动注册：以 ".grpd" 后缀名为键
REGISTER_READER(".grpd", GrpdMeshReader)

} // namespace PDCommon::IO
