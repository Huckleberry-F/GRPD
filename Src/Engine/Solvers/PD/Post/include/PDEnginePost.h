#ifndef SRC_ENGINE_SOLVERS_PD_POST_PDENGINEPOST_H
#define SRC_ENGINE_SOLVERS_PD_POST_PDENGINEPOST_H

#include "PDContext.h"
#include <string>

namespace Src::Engine::Solvers::PD::Post {

/// @brief 接管 PD 引擎的后处理逻辑（基于物理场与 IOManager 把场写出为 VTK）
/// @details 定位物理域内的所有已解算坐标以及状态变量，遵循 yaml 配置里定制
/// 的列表提取多维张量信息（标量/矢量），最后由底层的 Outputer 类导出到文件存储系统。
/// @param ctx PD 全局上下文
/// @param yamlPath 传入模拟设定的 yaml 路径以便按需剥取导出白名单
/// @param step 当前写入帧步编号（对应帧文件后缀）
/// @param physicalTime 传入写入时间进行日志打标
void ExportVTK(const PDCommon::Core::PDContext &ctx,
               const std::string &yamlPath,
               int step, double physicalTime);

} // namespace Src::Engine::Solvers::PD::Post

#endif // SRC_ENGINE_SOLVERS_PD_POST_PDENGINEPOST_H
