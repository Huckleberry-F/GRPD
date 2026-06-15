#ifndef PDCOMMON_IO_IOMANAGER_H
#define PDCOMMON_IO_IOMANAGER_H

// ============================================================================
// IOManager.h - 全局输入输出管理器 (单例模式)
//
// 设计理念：
//   IOManager 是整个 GRPD 求解器的"路径中枢"。它在程序启动时自动推导出
//   模型名称、配置文件路径、输出目录等信息，并通过单例接口向所有模块提供
//   统一的路径查询服务。
//
// 为什么用单例？
//   路径信息是全局唯一且与程序生命周期绑定的。任何模块（Engine, Outputer,
//   Reader）在任何时刻都可能需要查询路径，单例保证了访问便利性和一致性。
//
// 使用方法：
//   auto& io = PDCommon::IO::IOManager::getInstance();
//   io.initialize();  // 在 main() 中调用一次
//   std::string resultDir = io.getResultDir().string();
// ============================================================================

#include <filesystem>
#include <string>

namespace PDCommon::IO {

namespace fs = std::filesystem;

class IOManager {
public:
  // ---- 单例访问 ----
  static IOManager &getInstance();

  // 禁止拷贝和移动（单例标准做法）
  IOManager(const IOManager &) = delete;
  IOManager &operator=(const IOManager &) = delete;
  IOManager(IOManager &&) = delete;
  IOManager &operator=(IOManager &&) = delete;

  /// @brief 初始化 IOManager
  /// 自动完成：推导模型名、定位 PD.yaml、创建带时间戳的 Result 文件夹
  void initialize();

  // ---- 路径查询接口 ----

  /// @brief 获取用户 cd 进的工作目录（模型文件夹）
  const fs::path &getWorkDir() const;

  /// @brief 获取 PD.yaml 的绝对路径
  const fs::path &getYamlPath() const;

  /// @brief 获取本次运行的结果输出目录 (Result_YYYYMMDD_HHMMSS)
  const fs::path &getResultDir() const;

  /// @brief 获取模型名称（= 工作目录的文件夹名）
  const std::string &getModelName() const;

  /// @brief 获取软件安装根目录（exe 所在位置往上推导）
  const fs::path &getInstallDir() const;

  /// @brief 获取内置 Python 脚本的完整路径
  /// @param scriptName 脚本文件名，如 "generate_model.py"
  fs::path getScriptPath(const std::string &scriptName) const;

  /// @brief 获取 .grpd 文件的完整路径（自动从模型名推导）
  /// 例如： workDir / "Bunny_Thermal.grpd"
  fs::path getGrpdPath() const;

  /// @brief 在工作目录中查找已注册的网格文件
  /// 扫描 workDir，返回第一个匹配 ReaderRegistry 已注册后缀名的文件路径
  /// @return 找到的网格文件路径，未找到则返回空路径
  fs::path findMeshFile() const;

  // ---- 输出路径生成 ----

  /// @brief 根据物理时间生成 VTK 文件的完整路径
  /// @param baseName 文件基础名（通常为模型名）
  /// @param step 当前保留为兼容参数；文件名不再使用步号序列
  /// @param time 物理时间，作为 VTK 序列文件唯一变化后缀
  /// @return 完整的 VTK 文件路径字符串
  std::string buildVtkPath(const std::string &baseName, int step,
                           double time) const;

  /// @brief 在 Result 目录下生成任意文件的完整路径
  /// @param filename 文件名（如 "log.txt"）
  /// @return 完整的文件路径字符串
  std::string buildResultPath(const std::string &filename) const;

  /// @brief 检查 IOManager 是否已初始化
  bool isInitialized() const;

private:
  IOManager() = default;

  bool initialized_ = false;
  fs::path workDir_;      ///< 用户 cd 进的工作目录
  fs::path yamlPath_;     ///< PD.yaml 绝对路径
  fs::path resultDir_;    ///< 本次运行的 Result_时间戳 目录
  fs::path installDir_;   ///< 软件安装根目录（exe 所在的上级目录）
  std::string modelName_; ///< 模型名称 = 工作目录文件夹名
};

} // namespace PDCommon::IO

#endif // PDCOMMON_IO_IOMANAGER_H
