/********************************************************************************
 * *
 * Logger.cpp                                                                  *
 * By Huckelberry Zhang  2026-03-3     zhanghanbo@inspur.com                   *
 * *
 ********************************************************************************/
#include "Logger.h"
#include <iostream>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

#include <chrono>
#include <iomanip>
#include <sstream>

namespace {
const std::string RESET   = "\033[0m";
const std::string BLACK   = "\033[30m";
const std::string RED     = "\033[31m";
const std::string GREEN   = "\033[32m";
const std::string YELLOW  = "\033[33m";
const std::string BLUE    = "\033[34m";
const std::string MAGENTA = "\033[35m";
const std::string CYAN    = "\033[36m";
const std::string WHITE   = "\033[37m";
const std::string GRAY    = "\033[90m";

std::string getCurrentTimeStr() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%H:%M:%S");
    return ss.str();
}

std::vector<std::string> wordWrap(const std::string& text, size_t maxWidth) {
    std::vector<std::string> lines;
    std::stringstream ss(text);
    std::string line;
    
    auto getUtf8CharLength = [](unsigned char c) -> size_t {
        if ((c & 0x80) == 0) return 1;
        if ((c & 0xE0) == 0xC0) return 2;
        if ((c & 0xF0) == 0xE0) return 3;
        if ((c & 0xF8) == 0xF0) return 4;
        return 1;
    };

    while (std::getline(ss, line)) {
        if (line.empty()) {
            lines.push_back("");
            continue;
        }

        size_t firstNonSpace = line.find_first_not_of(' ');
        std::string indent = "";
        size_t indentCols = 0;
        if (firstNonSpace != std::string::npos && firstNonSpace > 0) {
            indent = line.substr(0, firstNonSpace);
            indentCols = firstNonSpace; // spaces count as 1 col each
        }

        size_t start = 0;
        bool isFirstChunkOfLine = true;
        
        while (start < line.length()) {
            size_t bytes = 0;
            size_t lastSpaceBytes = std::string::npos;
            
            std::string currentIndent = isFirstChunkOfLine ? "" : indent;
            size_t cols = isFirstChunkOfLine ? 0 : indentCols;

            while (start + bytes < line.length() && cols < maxWidth) {
                unsigned char c = line[start + bytes];
                size_t len = getUtf8CharLength(c);
                size_t charCols = (len == 1) ? 1 : 2; 

                if (cols + charCols > maxWidth) {
                    break;
                }

                if (c == ' ') {
                    lastSpaceBytes = bytes;
                }

                bytes += len;
                cols += charCols;
            }

            if (start + bytes >= line.length()) {
                lines.push_back(currentIndent + line.substr(start));
                break;
            }

            if (lastSpaceBytes != std::string::npos && lastSpaceBytes > 0) {
                lines.push_back(currentIndent + line.substr(start, lastSpaceBytes));
                start += lastSpaceBytes + 1;
            } else {
                if (bytes == 0) bytes = getUtf8CharLength(line[start]);
                lines.push_back(currentIndent + line.substr(start, bytes));
                start += bytes;
            }
            
            isFirstChunkOfLine = false;
        }
    }
    return lines;
}

static bool isFirstLog = true;

struct LogResult {
    std::string consoleMsg;
    std::string fileMsg;
};

LogResult formatLogMessage(const std::string &level, const std::string &message, 
                           std::string &lastModuleName, std::string &lastLevel) {
    std::string moduleName = "";
    std::string content = message;

    if (!message.empty() && message[0] == '[') {
        size_t endBracket = message.find(']');
        if (endBracket != std::string::npos) {
            moduleName = message.substr(0, endBracket + 1);
            size_t contentStart = endBracket + 1;
            while (contentStart < message.size() && std::isspace(message[contentStart])) {
                contentStart++;
            }
            content = message.substr(contentStart);
        }
    }

    // Inherit the last module name if not explicitly provided, to group logs smoothly
    if (moduleName.empty()) {
        moduleName = lastModuleName;
    }

    bool isRepeated = (!moduleName.empty() && moduleName == lastModuleName && level == lastLevel);
    bool moduleChanged = (!isFirstLog && !isRepeated && moduleName != lastModuleName);
    isFirstLog = false;
    
    if (!moduleName.empty()) {
        lastModuleName = moduleName;
    } else {
        lastModuleName = ""; 
    }
    lastLevel = level;

    std::string timeStr = getCurrentTimeStr();
    
    std::string levelPad = level;
    if (levelPad.size() < 5) levelPad.append(5 - levelPad.size(), ' ');

    std::string outFile;
    std::string outConsole;
    
    if (moduleChanged) {
        outFile += "\n";
        outConsole += "\n";
    }

    if (!isRepeated) {
        std::string headerFile = "[" + timeStr + "] [" + levelPad + "] ";
        std::string levelColor = GREEN;
        if (level == "ERROR") levelColor = RED;
        else if (level == "WARNING") levelColor = YELLOW;
        
        std::string headerConsole = GRAY + "[" + timeStr + "] " + RESET +
                                    "[" + levelColor + levelPad + RESET + "] ";
                        
        std::string modStrFile = moduleName.empty() ? "" : moduleName;
        std::string modStrConsole = moduleName.empty() ? "" : (CYAN + moduleName + RESET);
        
        headerFile += modStrFile;
        headerConsole += modStrConsole;
        
        outFile += headerFile + "\n";
        outConsole += headerConsole + "\n";
    }

    std::string indentStrFile = "    | ";
    std::string indentStrConsole = "    | ";

    std::vector<std::string> textLines = wordWrap(content, 100);
    
    bool isFirstLine = true;
    for (const auto& line : textLines) {
        if (!isFirstLine) {
            outFile += "\n";
            outConsole += "\n";
        }
        outFile += indentStrFile + line;
        outConsole += indentStrConsole + line;
        isFirstLine = false;
    }
    
    return { outConsole, outFile };
}

} // namespace

namespace PDCommon {
namespace Utils {
Logger &Logger::getInstance() {
  static Logger instance;
#ifdef _WIN32
  static bool ansi_enabled = false;
  if (!ansi_enabled) {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
      DWORD dwMode = 0;
      if (GetConsoleMode(hOut, &dwMode)) {
        dwMode |= 0x0004; // ENABLE_VIRTUAL_TERMINAL_PROCESSING
        SetConsoleMode(hOut, dwMode);
      }
    }
    ansi_enabled = true;
  }
#endif
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
  auto out = formatLogMessage("INFO", message, lastModuleName_, lastLevel_);
  std::cout << out.consoleMsg << std::endl;
  if (logFile_.is_open()) {
    logFile_ << out.fileMsg << std::endl;
  }
}

void Logger::warning(const std::string &message) {
  std::lock_guard<std::mutex> lock(logMutex_);
  auto out = formatLogMessage("WARNING", message, lastModuleName_, lastLevel_);
  std::cout << out.consoleMsg << std::endl;
  if (logFile_.is_open()) {
    logFile_ << out.fileMsg << std::endl;
  }
}

void Logger::error(const std::string &message) {
  std::lock_guard<std::mutex> lock(logMutex_);
  auto out = formatLogMessage("ERROR", message, lastModuleName_, lastLevel_);
  std::cerr << out.consoleMsg << std::endl;
  if (logFile_.is_open()) {
    logFile_ << out.fileMsg << std::endl;
  }
}

} // namespace Utils
} // namespace PDCommon
