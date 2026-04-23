#include "NTSEvaluator.h"
#include "ContactRegistry.h"
#include "ContactSpatialGrid.h"
#include "FieldManager.h"
#include "FieldRegistry.h"
#include "MechanicalMaterial.h"
#include "PDContext.h"
#include "ParticleManager.h"
#include <algorithm>
#include <cmath>

namespace PDCommon::Contact {

void NTSEvaluator::initialize(const YAML::Node &configNode) {
  if (configNode["PinballRatio"]) {
    pinballRatio_ = configNode["PinballRatio"].as<double>();
  }
  if (configNode["SmoothRatio"]) {
    smoothRatio_ = configNode["SmoothRatio"].as<double>();
  }
}

void NTSEvaluator::onPreEvaluate(PDCommon::Core::PDContext &ctx, double maxDx) {
  auto &fm = ctx.getFieldManager();

  auto *coordsField = fm.getFieldAs<double>("Coords");
  auto *dispField = fm.getFieldAs<double>("Displacement");
  auto *volField = fm.getFieldAs<double>("Volume");
  auto *velField = fm.getFieldAs<double>("Velocity");
  auto *isSurfaceField = fm.getFieldAs<int>("IsSurface");

  coords_ = coordsField ? coordsField->dataPtr() : nullptr;
  disp_ = dispField ? dispField->dataPtr() : nullptr;
  vols_ = volField ? volField->dataPtr() : nullptr;
  vel_ = velField ? velField->dataPtr() : nullptr;
  isSurface_ = isSurfaceField ? isSurfaceField->dataPtr() : nullptr;

  // 主动申请一个只用于输出可视化的场变量（如果已被其他层申请过则自动跳过）
  if (!fm.hasField("ContactNormal")) {
    auto &reg = PDCommon::Field::FieldRegistry::getInstance();
    auto normalFieldBase = reg.createField("DoubleField", "ContactNormal", 3);
    if (coordsField)
      normalFieldBase->resize(ctx.getParticleManager().getTotalParticles());
    fm.addField(std::move(normalFieldBase));
  }
  if (!fm.hasField("VirtualSurfacePos")) {
    auto &reg = PDCommon::Field::FieldRegistry::getInstance();
    auto vsField = reg.createField("DoubleField", "VirtualSurfacePos", 3);
    if (coordsField)
      vsField->resize(ctx.getParticleManager().getTotalParticles());
    fm.addField(std::move(vsField));
  }
  if (!fm.hasField("ContactGap")) {
    auto &reg = PDCommon::Field::FieldRegistry::getInstance();
    auto gapField = reg.createField("DoubleField", "ContactGap", 1);
    if (coordsField)
      gapField->resize(ctx.getParticleManager().getTotalParticles());
    fm.addField(std::move(gapField));
  }

  auto *normalField = fm.getFieldAs<double>("ContactNormal");
  if (normalField && coordsField && normalField->size() == 0) {
    normalField->resize(ctx.getParticleManager().getTotalParticles());
  }

  auto *vsField = fm.getFieldAs<double>("VirtualSurfacePos");
  if (vsField && coordsField && vsField->size() == 0) {
    vsField->resize(ctx.getParticleManager().getTotalParticles());
  }

  auto *gapField = fm.getFieldAs<double>("ContactGap");
  if (gapField && coordsField && gapField->size() == 0) {
    gapField->resize(ctx.getParticleManager().getTotalParticles());
  }

  // 只有在新的增量步（拓扑未冻结时）才清空这些场变量，防止遗留的僵尸脏数据误导结果。
  // 如果是子步中间的人工松弛迭代，应当保留其值以供最终输出或下一级算子调用。
  if (ctx.isIncrementStart()) {
    size_t numParticles = ctx.getParticleManager().getTotalParticles();
    if (normalField) {
      double *normalPtr = normalField->dataPtr();
      std::fill(normalPtr, normalPtr + numParticles * 3, 0.0);
    }
    if (gapField) {
      double *gapPtr = gapField->dataPtr();
      std::fill(gapPtr, gapPtr + numParticles, 0.0);
    }
    if (vsField) {
      double *vsPtr = vsField->dataPtr();
      // 这里如果初始没有被用到，可以用原始坐标xi或者直接0.0
      std::fill(vsPtr, vsPtr + numParticles * 3, 0.0);
    }
    frozenCache_.clear();
  }
}

std::vector<ContactContext>
NTSEvaluator::evaluate(int i, const ContactSpatialGrid &grid,
                       PDCommon::Core::PDContext &ctx) {
                       
  // =========================================================================
  // 【核心机制：子步内权重冻结 (Frozen Topology)】
  // 如果当前处于同一物理或伪时间子步的非起点迭代，且该粒子具备已冻结的拓扑缓存，
  // 则跳过所有的邻域搜索、法向重新拟合、权重更新，直接以常数雅可比计算位移偏差。
  // 这将非保守的 NTS 接触力场严格压制为全等二次保守力场，保障 ADR 收敛极值。
  // =========================================================================
  if (!ctx.isIncrementStart() && frozenCache_.count(i) > 0) {
    std::vector<ContactContext> cachedContexts = frozenCache_[i];
    std::vector<ContactContext> activeContexts;

    if (cachedContexts.empty()) return activeContexts;

    double xi = coords_[i * 3] + (disp_ ? disp_[i * 3] : 0.0);
    double yi = coords_[i * 3 + 1] + (disp_ ? disp_[i * 3 + 1] : 0.0);
    double zi = coords_[i * 3 + 2] + (disp_ ? disp_[i * 3 + 2] : 0.0);

    for (auto &cached : cachedContexts) {
      double P_surf = 0.0;
      double surf_w = 0.0;
      double vx_master = 0, vy_master = 0, vz_master = 0;

      // 利用冻结的权重和主节点，结合最新位移计算虚拟平滑主面
      for (const auto &wp : cached.master_weights) {
        int j = wp.first;
        double w = wp.second;
        double xj = coords_[j * 3] + (disp_ ? disp_[j * 3] : 0.0);
        double yj = coords_[j * 3 + 1] + (disp_ ? disp_[j * 3 + 1] : 0.0);
        double zj = coords_[j * 3 + 2] + (disp_ ? disp_[j * 3 + 2] : 0.0);

        P_surf += (xj * cached.nx + yj * cached.ny + zj * cached.nz) * w;
        surf_w += w;

        if (vel_) {
          vx_master += vel_[j * 3] * w;
          vy_master += vel_[j * 3 + 1] * w;
          vz_master += vel_[j * 3 + 2] * w;
        }
      }

      if (surf_w > 1e-14) {
        P_surf /= surf_w;
        vx_master /= surf_w;
        vy_master /= surf_w;
        vz_master /= surf_w;
      }

      double P_i = xi * cached.nx + yi * cached.ny + zi * cached.nz;
      double gap = P_i - P_surf;

      auto *vsField = ctx.getFieldManager().getFieldAs<double>("VirtualSurfacePos");
      if (vsField) {
        double *vsPtr = vsField->dataPtr();
        vsPtr[i * 3] = xi - gap * cached.nx;
        vsPtr[i * 3 + 1] = yi - gap * cached.ny;
        vsPtr[i * 3 + 2] = zi - gap * cached.nz;
      }
      auto *gapField = ctx.getFieldManager().getFieldAs<double>("ContactGap");
      if (gapField) gapField->dataPtr()[i] = gap;

      if (gap >= cached.safeDist) continue; // 冻结状态下依然发生脱离，跳过

      cached.dist = gap;
      cached.raw_penetration = std::min(cached.safeDist - gap, cached.safeDist * 0.5);

      if (vel_) {
        cached.dvx = vel_[i * 3] - vx_master;
        cached.dvy = vel_[i * 3 + 1] - vy_master;
        cached.dvz = vel_[i * 3 + 2] - vz_master;
      }

      activeContexts.push_back(cached);
    }
    return activeContexts;
  }

  std::vector<ContactContext> contexts;
  if (!coords_ || !vols_) {
    frozenCache_[i] = contexts;
    return contexts;
  }

  // 面面接触大过滤：如果你自己都不是表面皮粒子，那就老实躲在里头，不需要你参与接触
  if (isSurface_ && isSurface_[i] == 0)
    return contexts;

  double xi = coords_[i * 3] + (disp_ ? disp_[i * 3] : 0.0);
  double yi = coords_[i * 3 + 1] + (disp_ ? disp_[i * 3 + 1] : 0.0);
  double zi = coords_[i * 3 + 2] + (disp_ ? disp_[i * 3 + 2] : 0.0);

  const int dim = ctx.getDimension();
  const double thickness = ctx.getThickness();
  double vol_i = vols_[i];
  auto volToDx = [dim, thickness](double v) {
    return (dim == 2) ? std::sqrt(v / thickness) : std::cbrt(v);
  };
  double dx_i = volToDx(vol_i);

  // 广域平滑半径：不仅局限于刚好碰撞，而是要框住一个“足够代表该处几何曲面”的点云
  double smoothRadius = dx_i * smoothRatio_;

  std::vector<int> masterNodes;

  grid.forEachNeighbor(xi, yi, zi, [&](int j) {
    if (i == j)
      return; // NTS 通常不与自身的同 part 点进行自接触评价，由 GeneralContact
              // 在此层外筛选，或此处自己筛选
    double xj = coords_[j * 3] + (disp_ ? disp_[j * 3] : 0.0);
    double yj = coords_[j * 3 + 1] + (disp_ ? disp_[j * 3 + 1] : 0.0);
    double zj = coords_[j * 3 + 2] + (disp_ ? disp_[j * 3 + 2] : 0.0);
    double rx = xi - xj;
    double ry = yi - yj;
    double rz = zi - zj;
    double dist = std::sqrt(rx * rx + ry * ry + rz * rz);

    // 关键过滤 2：NTS 只应该用最纯粹的主面表皮（蒙皮点）来凝结算子面！
    if (isSurface_ && isSurface_[j] == 0)
      return;

    // 将搜索半径扩大到 smoothRadius，以建立大局观法向
    if (dist < smoothRadius + volToDx(vols_[j]) * pinballRatio_) {
      masterNodes.push_back(j);
    }
  });

  if (masterNodes.empty())
    return contexts;

  // 1. Calculate NTS Normal via Density Gradient (核函数引力法向)
  double nx_total = 0, ny_total = 0, nz_total = 0;
  double sum_w = 0;
  for (int j : masterNodes) {
    double xj = coords_[j * 3] + (disp_ ? disp_[j * 3] : 0.0);
    double yj = coords_[j * 3 + 1] + (disp_ ? disp_[j * 3 + 1] : 0.0);
    double zj = coords_[j * 3 + 2] + (disp_ ? disp_[j * 3 + 2] : 0.0);
    double rx = xi - xj;
    double ry = yi - yj;
    double rz = zi - zj;
    double distsq = rx * rx + ry * ry + rz * rz;
    if (distsq < 1e-14)
      continue;

    double vol_j = vols_[j];
    double dist = std::sqrt(distsq);
    double local_R = smoothRadius + volToDx(vol_j) * pinballRatio_;
    double r_norm = dist / local_R;
    if (r_norm >= 1.0)
      continue;

    // 采用抛物线平滑核函数 (1 - r/R)^2 取代具有 r->0 奇异性的 1/r^2
    // 经典距离反比，避免在网格正对齐时产生无穷大权重吸附
    double kernel = (1.0 - r_norm) * (1.0 - r_norm);
    double w = vol_j * kernel;

    nx_total += (rx / dist) * w;
    ny_total += (ry / dist) * w;
    nz_total += (rz / dist) * w;
    sum_w += w;
  }

  if (sum_w < 1e-14)
    return contexts;
  double n_mag = std::sqrt(nx_total * nx_total + ny_total * ny_total +
                           nz_total * nz_total);
  if (n_mag < 1e-12)
    return contexts;

  double nx = nx_total / n_mag;
  double ny = ny_total / n_mag;
  double nz = nz_total / n_mag;

  // 输出此步刚刚拟合好并且决定拿去计算的核函数面法向 (可视化用)
  auto *normalField = ctx.getFieldManager().getFieldAs<double>("ContactNormal");
  if (normalField) {
    double *normalPtr = normalField->dataPtr();
    // OMP 线程安全写入，因为各 thread 处理各自独占的 i
    normalPtr[i * 3] = nx;
    normalPtr[i * 3 + 1] = ny;
    normalPtr[i * 3 + 2] = nz;
  }

  // 2. Calculate Abstract Flat Surface Position P_surf
  // (虚拟主滑移参考平面的“平均高度”)
  double P_surf = 0.0;
  double surf_w = 0;
  for (int j : masterNodes) {
    double xj = coords_[j * 3] + (disp_ ? disp_[j * 3] : 0.0);
    double yj = coords_[j * 3 + 1] + (disp_ ? disp_[j * 3 + 1] : 0.0);
    double zj = coords_[j * 3 + 2] + (disp_ ? disp_[j * 3 + 2] : 0.0);
    double P_j = xj * nx + yj * ny + zj * nz; // j 节点在平滑法线上的投影高度

    // 距离面越近的节点，对面高的决策权越重
    double distsq =
        (xi - xj) * (xi - xj) + (yi - yj) * (yi - yj) + (zi - zj) * (zi - zj);
    double dist = std::sqrt(distsq);
    double local_R = smoothRadius + volToDx(vols_[j]) * pinballRatio_;
    double r_norm = dist / std::max(local_R, 1e-12);
    double kernel = (r_norm < 1.0) ? (1.0 - r_norm) * (1.0 - r_norm) : 0.0;
    double w = vols_[j] * kernel;

    P_surf += P_j * w;
    surf_w += w;
  }
  P_surf /= surf_w;

  // 3. Project slave i and get Gap (计算从面投影点与平滑曲面的垂向差距)
  double P_i = xi * nx + yi * ny + zi * nz;

  // NTS 抽象主面的等效体积安全限
  double avg_dx_j = 0;
  for (int j : masterNodes) {
    double cxj = coords_[j * 3] + (disp_ ? disp_[j * 3] : 0.0);
    double cyj = coords_[j * 3 + 1] + (disp_ ? disp_[j * 3 + 1] : 0.0);
    double czj = coords_[j * 3 + 2] + (disp_ ? disp_[j * 3 + 2] : 0.0);
    double distsq = (xi - cxj) * (xi - cxj) + (yi - cyj) * (yi - cyj) +
                    (zi - czj) * (zi - czj);
    double dist = std::sqrt(distsq);
    double local_R = smoothRadius + volToDx(vols_[j]) * pinballRatio_;
    double r_norm = dist / std::max(local_R, 1e-12);
    double kernel = (r_norm < 1.0) ? (1.0 - r_norm) * (1.0 - r_norm) : 0.0;
    double w = vols_[j] * kernel;
    avg_dx_j += volToDx(vols_[j]) * w;
  }
  avg_dx_j /= surf_w;

  double safeDist = (dx_i + avg_dx_j) * 0.5 * pinballRatio_;
  double gap = P_i - P_surf;

  // 记录刚刚计算出的 Virtual Surface 3D 落点坐标和 Gap 量，以供排查虚拟面波动
  auto *vsField = ctx.getFieldManager().getFieldAs<double>("VirtualSurfacePos");
  if (vsField) {
    double *vsPtr = vsField->dataPtr();
    vsPtr[i * 3] = xi - gap * nx;
    vsPtr[i * 3 + 1] = yi - gap * ny;
    vsPtr[i * 3 + 2] = zi - gap * nz;
  }
  auto *gapField = ctx.getFieldManager().getFieldAs<double>("ContactGap");
  if (gapField) {
    gapField->dataPtr()[i] = gap;
  }

  if (gap >= safeDist)
    return contexts; // 如果从面飘在虚拟平滑云的上方安全距离外，毫无穿透

  double penetration = safeDist - gap; // 穿刺了多少
  // 限幅保护：防止 Slave 过深侵入 Master 导致密度梯度法向摇摆（NTS
  // 极限环震荡的根因） Kinematic 法则另行处理，此处针对 Penalty
  // 类法则做稳定性保护
  penetration = std::min(penetration, safeDist * 0.5);

  // 4. Calculate smoothed master velocity array
  // (提取主面网格的随质点运动平滑速度)
  double dvx = 0, dvy = 0, dvz = 0;
  if (vel_) {
    double vx_master = 0, vy_master = 0, vz_master = 0;
    for (int j : masterNodes) {
      double cxj = coords_[j * 3] + (disp_ ? disp_[j * 3] : 0.0);
      double cyj = coords_[j * 3 + 1] + (disp_ ? disp_[j * 3 + 1] : 0.0);
      double czj = coords_[j * 3 + 2] + (disp_ ? disp_[j * 3 + 2] : 0.0);
      double distsq = (xi - cxj) * (xi - cxj) + (yi - cyj) * (yi - cyj) +
                      (zi - czj) * (zi - czj);
      double dist = std::sqrt(distsq);
      double local_R = smoothRadius + volToDx(vols_[j]) * pinballRatio_;
      double r_norm = dist / std::max(local_R, 1e-12);
      double kernel = (r_norm < 1.0) ? (1.0 - r_norm) * (1.0 - r_norm) : 0.0;
      double w = vols_[j] * kernel;
      vx_master += vel_[j * 3] * w;
      vy_master += vel_[j * 3 + 1] * w;
      vz_master += vel_[j * 3 + 2] * w;
    }
    vx_master /= surf_w;
    vy_master /= surf_w;
    vz_master /= surf_w;

    dvx = vel_[i * 3] - vx_master;
    dvy = vel_[i * 3 + 1] - vy_master;
    dvz = vel_[i * 3 + 2] - vz_master;
  }

  // 5. Package context
  auto &pm = ctx.getParticleManager();
  const auto &particles = pm.getAllParticles();

  auto *mat_i = dynamic_cast<PDCommon::Material::MechanicalMaterial *>(
      particles[i].getMaterial());
  double rho_i = mat_i ? mat_i->getDensity() : 1.0;
  double mass_i = rho_i * vol_i * massScaleFactor_;

  int j_rep = masterNodes[0];
  auto *mat_master = dynamic_cast<PDCommon::Material::MechanicalMaterial *>(
      particles[j_rep].getMaterial());
  // Abstract mass representation (考虑2D/3D体积的正确计算)
  double master_vol = (dim == 2) ? (avg_dx_j * avg_dx_j * thickness)
                                 : (avg_dx_j * avg_dx_j * avg_dx_j);
  double mass_master = (mat_master ? mat_master->getDensity() : 1.0) *
                       master_vol * massScaleFactor_;

  ContactContext cx;
  cx.i = i;
  cx.j = j_rep;  // 借用代表点提取主面材料
  cx.dist = gap; // 投影距离
  cx.nx = nx;
  cx.ny = ny;
  cx.nz = nz;
  cx.raw_penetration = penetration;
  cx.safeDist = safeDist;
  cx.dvx = dvx;
  cx.dvy = dvy;
  cx.dvz = dvz;
  cx.mass_i = mass_i;
  cx.mass_j = mass_master;
  cx.dx_i = dx_i;
  cx.dx_j = avg_dx_j;

  // 分配受力反弹权重（依据逆距离插值平滑分配惩罚力）
  for (int j : masterNodes) {
    double cxj = coords_[j * 3] + (disp_ ? disp_[j * 3] : 0.0);
    double cyj = coords_[j * 3 + 1] + (disp_ ? disp_[j * 3 + 1] : 0.0);
    double czj = coords_[j * 3 + 2] + (disp_ ? disp_[j * 3 + 2] : 0.0);
    double distsq = (xi - cxj) * (xi - cxj) + (yi - cyj) * (yi - cyj) +
                    (zi - czj) * (zi - czj);
    double dist = std::sqrt(distsq);
    double local_R = smoothRadius + volToDx(vols_[j]) * pinballRatio_;
    double r_norm = dist / std::max(local_R, 1e-12);
    double kernel = (r_norm < 1.0) ? (1.0 - r_norm) * (1.0 - r_norm) : 0.0;
    double w = vols_[j] * kernel;
    cx.master_weights.push_back({j, w / surf_w});
  }

  contexts.push_back(cx);
  frozenCache_[i] = contexts;
  return contexts;
}

} // namespace PDCommon::Contact

REGISTER_EVALUATOR_TYPE(NTS, NTSEvaluator)
