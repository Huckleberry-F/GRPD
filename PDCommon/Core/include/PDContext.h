#ifndef PDCOMMON_CORE_PDCONTEXT_H
#define PDCOMMON_CORE_PDCONTEXT_H

// ============================================================================
// PDContext.h - 杩戝満鍔ㄥ姏瀛︿豢鐪熷ぇ绠″ (鏍稿績鏋㈢航)
// 璐ｄ换锛?
// 1. 浠ｈ〃涓€涓畬鏁寸殑 PD 鐗╃悊鎴栬绠楀伐绋嬩笂涓嬫枃
// 2. 鐙珛鎸佹湁鑷繁涓撳睘鐨?ParticleManager銆丮aterialManager銆丗ieldManager 绛夋暟鎹粍浠?
// 3. 浣滀负 PD 绯荤粺涓庡鐣?(濡備富绋嬪簭銆佹垨鑰呮湭鏉ョ殑 FEM 绯荤粺) 鑰﹀悎鐨勫敮涓€鏁版嵁妗ユ
// ============================================================================

#include "MaterialManager.h"
#include "ParticleManager.h"
#include "FieldManager.h"
#include "NeighborList.h"
#include "BCManager.h"
#include <memory>
#include <string>

namespace PDCommon::Core {

class PDContext {
public:
  /// @brief 榛樿鏋勯€犲嚱鏁?
  PDContext() = default;

  /// @brief 鏋勯€犲嚱鏁帮紝鍒濆鍖栦竴涓叏鏂扮殑浠跨湡妯″瀷涓婁笅鏂?
  /// @param name 妯″瀷鍚嶇О锛岀敤浜庢爣璇嗗拰鏃ュ織杈撳嚭
  explicit PDContext(const std::string &name);

  ~PDContext() = default;

  // 绂佺敤鎷疯礉锛屽厑璁哥Щ鍔紙淇濇寔璧勬簮鐙崰锛?
  PDContext(const PDContext &) = delete;
  PDContext &operator=(const PDContext &) = delete;
  PDContext(PDContext &&) = default;
  PDContext &operator=(PDContext &&) = default;

  /// @brief 鑾峰彇妯″瀷鍚嶇О
  const std::string &getName() const { return name_; }

  /// @brief 璁剧疆妯″瀷鍚嶇О
  void setName(const std::string &name) { name_ = name; }

  /// @brief 鑾峰彇绮掑瓙绠＄悊鍣ㄥ紩鐢紙闈?const锛岀敤浜庢眰瑙ｅ拰鍒濆鍖栵級
  PDCommon::Model::ParticleManager &getParticleManager() {
    return particleManager_;
  }
  const PDCommon::Model::ParticleManager &getParticleManager() const {
    return particleManager_;
  }

  /// @brief 鑾峰彇鏉愭枡绠＄悊鍣ㄥ紩鐢紙闈?const锛岀敤浜庣粦瀹氬拰姹傝В锛?
  PDCommon::Material::MaterialManager &getMaterialManager() {
    return materialManager_;
  }
  const PDCommon::Material::MaterialManager &getMaterialManager() const {
    return materialManager_;
  }

  // -----------------------------------------------------------------------
  // 鐗╃悊鍦虹鐞嗗櫒 (FieldManager) 鈥?缁熶竴绠＄悊鎵€鏈夌墿鐞嗗満
  // -----------------------------------------------------------------------

  /// @brief 鑾峰彇鐗╃悊鍦虹鐞嗗櫒寮曠敤
  PDCommon::Field::FieldManager &getFieldManager() { return fieldManager_; }
  const PDCommon::Field::FieldManager &getFieldManager() const {
    return fieldManager_;
  }

  // -----------------------------------------------------------------------
  // 杩戦偦鍒楄〃 (NeighborList) 鎸夐渶鍒嗛厤
  // -----------------------------------------------------------------------
  void createNeighborList(const PDCommon::Model::ParticleManager &mgr,
                         double horizon);
  bool hasNeighborList() const { return neighborList_ != nullptr; }
  PDCommon::Initial::NeighborList &getNeighborList() { return *neighborList_; }
  const PDCommon::Initial::NeighborList &getNeighborList() const {
    return *neighborList_;
  }

  // -----------------------------------------------------------------------
  // 杈圭晫鏉′欢绠＄悊鍣?(BCManager)
  // -----------------------------------------------------------------------
  PDCommon::BC::BCManager &getBCManager() { return bcManager_; }
  const PDCommon::BC::BCManager &getBCManager() const { return bcManager_; }

private:
  std::string name_;                             // 妯″瀷鍚嶇О
  PDCommon::Model::ParticleManager particleManager_; // 璇ユā鍨嬩笓灞炵殑绮掑瓙鏁扮粍涓庣鐞嗗櫒
  PDCommon::Material::MaterialManager materialManager_; // 璇ユā鍨嬩笓灞炵殑鏉愭枡瀹炰緥绠＄悊
  PDCommon::Field::FieldManager fieldManager_;       // 鐗╃悊鍦虹鐞嗗櫒 (缁熶竴绠＄悊鎵€鏈夊満鏁版嵁)

  std::unique_ptr<PDCommon::Initial::NeighborList>
      neighborList_; // 杩戦偦鍒楄〃锛堟寜闇€鍒嗛厤锛?

  PDCommon::BC::BCManager bcManager_; // 杈圭晫鏉′欢绠＄悊鍣?(闈欐€佸垎閰?
};

} // namespace PDCommon::Core

#endif // PDCOMMON_CORE_PDCONTEXT_H
