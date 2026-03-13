/********************************************************************************
 * *
 * Logger.cpp                                                                  *
 * By Huckelberry Zhang  2026-03-3     zhanghanbo@inspur.com                   *
 * *
 ********************************************************************************/
#include "Logger.h"
#include <iostream>

namespace PDCommon {
namespace Utils {
Logger &Logger::getInstance() {
  static Logger instance;
  return instance;
}

Logger::~Logger() {
  if (logFile_.is_open()) {
    logFile_.close();
  }
}

void Logger::setLogFile(const std::string &logFilePath) {
  std::lock_guard<std::mutex> lock(logMutex_);
  if (logFile_.is_open()) {
    logFile_.close();
  }

  // Open file in append mode to ensure continuous logging across sessions
  logFile_.open(logFilePath, std::ios::app);
  if (!logFile_.is_open()) {
    std::cerr << "[WARNING] Failed to open log file: " << logFilePath
              << ", subsequent logs will only be output to the console."
              << std::endl;
  }
}

std::vector<std::string> Logger::buildLogoLines() {
  std::vector<std::string> lines;

  // ====== Logo 1: GRPD ======
  lines.push_back(
      "*****************************************************************");
  lines.push_back(
      "   ____  ____   ____   ____                                     ");
  lines.push_back(
      "  / ___||  _ \\ |  _ \\ |  _ \\                                   ");
  lines.push_back(
      " | |  _ | |_) || |_) || | | |                                  ");
  lines.push_back(
      " | |_| ||  _ < |  __/ | |_| |     v1.0                         ");
  lines.push_back(
      "  \\____||_| \\_\\|_|    |____/                                  ");
  lines.push_back(
      "*****************************************************************");
  lines.push_back("");

  // ====== Logo 2: GRPD (Block Style) ======
  lines.push_back(
      "*****************************************************************");
  lines.push_back(
      "    ####   ####    ####   ####                                  ");
  lines.push_back(
      "   ##  ## ##  ##  ## ## ##  ##                                  ");
  lines.push_back(
      "   ##     ##  ##  ##  # ##  ##                                  ");
  lines.push_back(
      "   ## ### #####   ####  ##  ##                                  ");
  lines.push_back(
      "   ##  ## ##  ##  ##    ##  ##                                  ");
  lines.push_back(
      "   ##  ## ##  ##  ##    ##  ##                                  ");
  lines.push_back(
      "    ####  ##  ##  ##    ####                                    ");
  lines.push_back(
      "*****************************************************************");
  lines.push_back("");

  // ====== Logo 3: Pipe Style GrPD ======
  lines.push_back("          _____                    _____                    "
                  "_____                    _____          ");
  lines.push_back("         /\\    \\                  /\\    \\               "
                  "   /\\    \\                  /\\    \\         ");
  lines.push_back("        /::\\    \\                /::\\    \\              "
                  "  /::\\    \\                /::\\    \\        ");
  lines.push_back("       /::::\\    \\              /::::\\    \\             "
                  " /::::\\    \\              /::::\\    \\       ");
  lines.push_back("      /::::::\\    \\            /::::::\\    \\            "
                  "/::::::\\    \\            /::::::\\    \\      ");
  lines.push_back("     /:::/\\:::\\    \\          /:::/\\:::\\    \\         "
                  " /:::/\\:::\\    \\          /:::/\\:::\\    \\     ");
  lines.push_back("    /:::/  \\:::\\    \\        /:::/__\\:::\\    \\        "
                  "/:::/__\\:::\\    \\        /:::/  \\:::\\    \\    ");
  lines.push_back("   /:::/    \\:::\\    \\      /::::\\   \\:::\\    \\      "
                  "/::::\\   \\:::\\    \\      /:::/    \\:::\\    \\   ");
  lines.push_back("  /:::/    / \\:::\\    \\    /::::::\\   \\:::\\    \\    "
                  "/::::::\\   \\:::\\    \\    /:::/    / \\:::\\    \\  ");
  lines.push_back(" /:::/    /   \\:::\\ ___\\  /:::/\\:::\\   \\:::\\____\\  "
                  "/:::/\\:::\\   \\:::\\____\\  /:::/    /   \\:::\\ ___\\ ");
  lines.push_back("/:::/____/  ___\\:::|    |/:::/  \\:::\\   \\:::|    |/:::/ "
                  " \\:::\\   \\:::|    |/:::/____/     \\:::|    |");
  lines.push_back(
      "\\:::\\    \\ /\\  /:::|____|\\::/   |::::\\  /:::|____|\\::/    "
      "\\:::\\  /:::|____|\\:::\\    \\     /:::|____|");
  lines.push_back(" \\:::\\    /::\\ \\::/    /  \\/____|:::::\\/:::/    /  "
                  "\\/_____/\\:::\\/:::/    /  \\:::\\    \\   /:::/    / ");
  lines.push_back("  \\:::\\   \\:::\\ \\/____/         |:::::::::/    /       "
                  "     \\::::::/    /    \\:::\\    \\ /:::/    /  ");
  lines.push_back("   \\:::\\   \\:::\\____\\           |::|\\::::/    /       "
                  "       \\::::/    /      \\:::\\    /:::/    /   ");
  lines.push_back("    \\:::\\  /:::/    /           |::| \\::/____/           "
                  "     \\::/____/        \\:::\\  /:::/    /    ");
  lines.push_back("     \\:::\\/:::/    /            |::|  ~|                  "
                  "     ~~               \\:::\\/:::/    /     ");
  lines.push_back("      \\::::::/    /             |::|   |                   "
                  "                      \\::::::/    /      ");
  lines.push_back("       \\::::/    /              \\::|   |                  "
                  "                        \\::::/    /       ");
  lines.push_back("        \\::/____/                \\:|   |                  "
                  "                         \\::/____/        ");
  lines.push_back("                                  \\|___|                   "
                  "                         ~~              ");
  lines.push_back("");

  // ====== Logo 4: Letter-based Pattern ======
  lines.push_back(
      "*****************************************************************");
  lines.push_back("   GGGGGGG RRRRRR  PPPPPP   DDDDD                        ");
  lines.push_back("   GG      RR  RR  PP   PP  DD  DD                       ");
  lines.push_back("   GG      RR  RR  PP   PP  DD   DD                      ");
  lines.push_back("   GG GGG  RRRRRR  PPPPPP   DD   DD                     ");
  lines.push_back("   GG  GG  RR RR   PP       DD   DD                     ");
  lines.push_back("   GG  GG  RR  RR  PP       DD  DD                      ");
  lines.push_back("   GGGGGGG RR   RR PP       DDDDD                       ");
  lines.push_back(
      "*****************************************************************");
  lines.push_back("");

  return lines;
}

void Logger::printToStreams(const std::vector<std::string> &lines) {
  std::lock_guard<std::mutex> lock(logMutex_);

  // Use '\n' instead of endl for efficient line breaks inside loops, avoiding
  // frequent buffer flushes
  for (const auto &line : lines) {
    std::cout << line << "\n";
    if (logFile_.is_open()) {
      logFile_ << line << "\n";
    }
  }

  // Flush the buffer once after the loop completes
  std::cout << std::endl;
  if (logFile_.is_open()) {
    logFile_ << std::endl;
  }
}

void Logger::showLogo() {
  auto lines = buildLogoLines();
  printToStreams(lines);
}

void Logger::info(const std::string &message) {
  std::lock_guard<std::mutex> lock(logMutex_);
  std::string outMsg = "[INFO] " + message;
  std::cout << outMsg << std::endl;
  if (logFile_.is_open()) {
    logFile_ << outMsg << std::endl;
  }
}

void Logger::warning(const std::string &message) {
  std::lock_guard<std::mutex> lock(logMutex_);
  std::string outMsg = "[WARNING] " + message;
  std::cout << outMsg << std::endl;
  if (logFile_.is_open()) {
    logFile_ << outMsg << std::endl;
  }
}

void Logger::error(const std::string &message) {
  std::lock_guard<std::mutex> lock(logMutex_);
  std::string outMsg = "[ERROR] " + message;
  std::cerr << outMsg << std::endl;
  if (logFile_.is_open()) {
    logFile_ << outMsg << std::endl;
  }
}

} // namespace Utils
} // namespace PDCommon
