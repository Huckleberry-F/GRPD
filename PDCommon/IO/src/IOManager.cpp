// ============================================================================
// IOManager.cpp - 全局输入输出管理器实现
// ============================================================================

#include "IOManager.h"
#include "Logger.h"
#include "ReaderRegistry.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

namespace PDCommon::IO {

// ---------------------------------------------------------------------------
// 单例实例获取（Meyers' Singleton，线程安全，C++11 保证）
// ---------------------------------------------------------------------------
IOManager &IOManager::getInstance() {
  static IOManager instance;
  return instance;
}

// ---------------------------------------------------------------------------
// 初始化：自动推导所有路径并创建输出目录
// ---------------------------------------------------------------------------
void IOManager::initialize() {
  if (initialized_) {
    LOG_WARNING("[IOManager] Already initialized. Skipping re-initialization.");
    return;
  }

  LOG_INFO("[IOManager] Initializing IO Manager...");

  // ===========================================================
  // 1. 获取当前工作目录（用户 cd 进的模型文件夹）
  // ===========================================================
  workDir_ = fs::current_path();
  LOG_INFO("[IOManager] Working directory: " + workDir_.string());

  // ===========================================================
  // 1.5 推导软件安装根目录（基于 exe 自身位置）
  //     exe 在 bin/release/GRPD.exe，往上两层 = 项目根目录
  // ===========================================================
#ifdef _WIN32
  wchar_t exeBuf[MAX_PATH];
  GetModuleFileNameW(nullptr, exeBuf, MAX_PATH);
  fs::path exePath(exeBuf);
  installDir_ = exePath.parent_path().parent_path().parent_path();
#elif defined(__APPLE__)
  char path[1024];
  uint32_t size = sizeof(path);
  if (_NSGetExecutablePath(path, &size) == 0) {
    fs::path exePath(path);
    installDir_ = exePath.parent_path().parent_path().parent_path();
  } else {
    installDir_ = fs::current_path();
  }
#else
  fs::path exePath = fs::read_symlink("/proc/self/exe");
  installDir_ = exePath.parent_path().parent_path().parent_path();
#endif
  LOG_INFO("[IOManager] Install directory: " + installDir_.string());

  // ===========================================================
  // 2. 从文件夹名推导模型名称
  //    例如：D:\Models\Bunny_Thermal → modelName_ = "Bunny_Thermal"
  // ===========================================================
  modelName_ = workDir_.filename().string();
  LOG_INFO("[IOManager] Model name: " + modelName_);

  // ===========================================================
  // 3. 检查 PD.yaml 是否存在
  // ===========================================================
  yamlPath_ = workDir_ / "PD.yaml";
  if (!fs::exists(yamlPath_)) {
    LOG_ERROR("[IOManager] Configuration file not found: " +
              yamlPath_.string());
    LOG_ERROR("[IOManager] Please make sure 'PD.yaml' exists in the current "
              "working directory.");
    exit(EXIT_FAILURE);
  }
  LOG_INFO("[IOManager] Config file found: " + yamlPath_.string());

  // ===========================================================
  // 4. 生成带时间戳的结果文件夹：Result_YYYYMMDD_HHMMSS
  //    使用 <chrono> + localtime 生成精确到秒的时间戳
  // ===========================================================
  auto now = std::chrono::system_clock::now();
  std::time_t now_time = std::chrono::system_clock::to_time_t(now);
  std::tm local_tm{};

#ifdef _WIN32
  localtime_s(&local_tm, &now_time);
#else
  localtime_r(&now_time, &local_tm);
#endif

  char timeBuffer[64];
  std::snprintf(timeBuffer, sizeof(timeBuffer), "Result_%04d%02d%02d_%02d%02d%02d",
                local_tm.tm_year + 1900, local_tm.tm_mon + 1, local_tm.tm_mday,
                local_tm.tm_hour, local_tm.tm_min, local_tm.tm_sec);

  resultDir_ = workDir_ / std::string(timeBuffer);

  // 创建输出目录（如果不存在）
  if (!fs::exists(resultDir_)) {
    fs::create_directories(resultDir_);
  }
  LOG_INFO("[IOManager] Result directory created: " + resultDir_.string());

  // ===========================================================
  // 5. 汇总输出
  // ===========================================================
  LOG_INFO("[IOManager] ==================================================");
  LOG_INFO("[IOManager] IO Manager Ready:");
  LOG_INFO("[IOManager]   Model Name  : " + modelName_);
  LOG_INFO("[IOManager]   Work Dir    : " + workDir_.string());
  LOG_INFO("[IOManager]   Install Dir : " + installDir_.string());
  LOG_INFO("[IOManager]   YAML Path   : " + yamlPath_.string());
  LOG_INFO("[IOManager]   Result Dir  : " + resultDir_.string());
  LOG_INFO("[IOManager] ==================================================");

  initialized_ = true;
}

// ---------------------------------------------------------------------------
// 路径查询接口
// ---------------------------------------------------------------------------
const fs::path &IOManager::getWorkDir() const { return workDir_; }

const fs::path &IOManager::getYamlPath() const { return yamlPath_; }

const fs::path &IOManager::getResultDir() const { return resultDir_; }

const std::string &IOManager::getModelName() const { return modelName_; }

const fs::path &IOManager::getInstallDir() const { return installDir_; }

fs::path IOManager::getScriptPath(const std::string &scriptName) const {
  // 脚本存放在项目根目录的 Generate_py/ 下
  return installDir_ / "Generate_py" / scriptName;
}

fs::path IOManager::getGrpdPath() const {
  return workDir_ / (modelName_ + ".grpd");
}

fs::path IOManager::findMeshFile() const {
  // 获取所有已注册的后缀名
  auto registeredExts = ReaderRegistry::getInstance().getRegisteredTypes();

  // 遍历工作目录，查找匹配的网格文件
  for (const auto &entry : fs::directory_iterator(workDir_)) {
    if (!entry.is_regular_file())
      continue;
    std::string ext = entry.path().extension().string();
    for (const auto &regExt : registeredExts) {
      if (ext == regExt) {
        LOG_INFO("[IOManager] Found mesh file: " + entry.path().string());
        return entry.path();
      }
    }
  }

  LOG_WARNING("[IOManager] No mesh file found in: " + workDir_.string());
  return fs::path{};
}

bool IOManager::isInitialized() const { return initialized_; }

// ---------------------------------------------------------------------------
// 输出路径生成
// ---------------------------------------------------------------------------
std::string IOManager::buildVtkPath(const std::string &baseName, int step,
                                    double time) const {
  // 生成格式：Result_时间戳/baseName_step00100_t0.0100.vtk
  char buffer[256];
  std::snprintf(buffer, sizeof(buffer), "%s_step%05d_t%.4f.vtk",
                baseName.c_str(), step, time);
  return (resultDir_ / std::string(buffer)).string();
}

std::string IOManager::buildResultPath(const std::string &filename) const {
  return (resultDir_ / filename).string();
}

} // namespace PDCommon::IO
