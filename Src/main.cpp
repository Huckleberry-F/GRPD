#include "EngineManager.h"
#include "GRPD.h"
#include "IOManager.h"
#include "Logger.h"
#include <cstdlib>

int main(int argc, char *argv[]) {

  // =================================================================
  // 1. 显示 Logo
  // =================================================================
  Start();

  // =================================================================
  // 2. 初始化 IOManager：自动推导路径、定位 PD.yaml、创建 Result 文件夹
  // =================================================================
  auto &io = PDCommon::IO::IOManager::getInstance();
  io.initialize();

  // 将日志文件输出到工作目录（Job 文件夹根目录）
  LOG_SET_FILE((io.getWorkDir() / "GRPD.log").string());

  // =================================================================
  // 3. 引擎调度：将 IOManager 提供的 YAML 路径传入引擎管理器
  // =================================================================
  Src::Engine::EngineManager GRSIM;
  GRSIM.Setup(io.getYamlPath().string());

  GRSIM.RunAll();

  GRSIM.ExportAll();

  // 注释掉 system("pause")，防止后台自动化回归测试及 CI 挂起
  // system("pause");

  return 0;
}
