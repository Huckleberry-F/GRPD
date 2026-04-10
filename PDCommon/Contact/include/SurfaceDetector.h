#ifndef PDCOMMON_CONTACT_SURFACEDETECTOR_H
#define PDCOMMON_CONTACT_SURFACEDETECTOR_H

// ============================================================================
// SurfaceDetector.h - 表面粒子识别工具类
// 责任：
// 1. 基于体积缺损法 (Volume Deficit Method) 静态标记系统外表面。
// 2. 将标记结果直接写入 FieldManager 中 (名为 "IsSurface" 的场)。
// 3. 作为独立工具类提供给引擎初始化阶段调用，保持 ContactManager 纯净度。
// ============================================================================

namespace PDCommon::Core {
class PDContext;
}

namespace PDCommon::Contact {

class SurfaceDetector {
public:
  /// @brief 使用体积缺损法 (Volume Deficit Method) 静态标记表面粒子
  /// 会在 FieldManager 中创建/更新名为 "IsSurface" 的 IntField (1=表面, 0=内部)
  ///
  /// 基本算法逻辑：
  /// - 遍历所有粒子，计算其近邻点体积之和 (Sum Volume)。
  /// - 提取全场最大的体积总和 (Max Sum Volume)。
  /// - 凡近邻缺损达到阈值（即 Sum Volume < threshold * Max Sum) 的记为表面。
  ///
  /// @param ctx 全局上下文
  /// @param threshold 体积缺损阈值（默认 0.95）
  static void identifySurfaceParticles(PDCommon::Core::PDContext &ctx,
                                       double threshold = 0.95);

  /// @brief (保留接口) 动态更新由于断裂暴露出来的新生表面粒子
  static void updateFracturedSurfaces(PDCommon::Core::PDContext &ctx);
};

} // namespace PDCommon::Contact

#endif // PDCOMMON_CONTACT_SURFACEDETECTOR_H
