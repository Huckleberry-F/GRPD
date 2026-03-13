/********************************************************************************
 * *
 * Logger.h                                                                    *
 * By Huckelberry Zhang  2026-03-3     zhanghanbo@inspur.com                   *
 * *
 ********************************************************************************/
#pragma once // Replaces #ifndef ... #define ...

#include <fstream>
#include <mutex>
#include <string>
#include <vector>

namespace PDCommon {
namespace Utils {

/**
 * @brief Global logger manager (Singleton pattern)
 * Handles all log output (console and file) and Logo display.
 * Thread-safe for OpenMP multi-threading.
 */
class Logger {
public:
  // Get the global singleton instance (C++11 static local guarantees
  // thread-safe init)
  static Logger &getInstance();

  // Disable copy and assignment
  Logger(const Logger &) = delete;
  Logger &operator=(const Logger &) = delete;

  // Set and open the global log file (console-only if not set)
  void setLogFile(const std::string &logFilePath);

  // Display the software Logo
  void showLogo();

  // Basic logging interface
  void info(const std::string &message);
  void warning(const std::string &message);
  void error(const std::string &message);

private:
  Logger() = default;
  ~Logger();

  // Internal: build all Logo text lines
  std::vector<std::string> buildLogoLines();

  // Internal: safely write block text to all active streams
  void printToStreams(const std::vector<std::string> &lines);

  std::mutex logMutex_;   // Output synchronization lock
  std::ofstream logFile_; // Global file stream instance
};

// Utility macros for cleaner call-site syntax
#define LOG_SET_FILE(path)                                                     \
  PDCommon::Utils::Logger::getInstance().setLogFile(path)
#define LOG_INFO(msg) PDCommon::Utils::Logger::getInstance().info(msg)
#define LOG_WARNING(msg) PDCommon::Utils::Logger::getInstance().warning(msg)
#define LOG_ERROR(msg) PDCommon::Utils::Logger::getInstance().error(msg)
#define SHOW_LOGO() PDCommon::Utils::Logger::getInstance().showLogo()

} // namespace Utils
} // namespace PDCommon
