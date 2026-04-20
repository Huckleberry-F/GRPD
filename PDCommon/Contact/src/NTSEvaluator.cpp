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

  auto *normalField = fm.getFieldAs<double>("ContactNormal");
  if (normalField && coordsField) {
    double *normalPtr = normalField->dataPtr();
    // 在计算每步前，先把上一针留下的法向痕迹清空
    size_t numParticles = ctx.getParticleManager().getTotalParticles();
    std::fill(normalPtr, normalPtr + numParticles * 3, 0.0);
  }
}

std::vector<ContactContext>
NTSEvaluator::evaluate(int i, const ContactSpatialGrid &grid,
                       PDCommon::Core::PDContext &ctx) {
  std::vector<ContactContext> contexts;
  if (!coords_ || !vols_)
    return contexts;

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
    // 经典 NTS 核函数：距离越近的权重越大，定义该处的局部垂线 (1/r^2 衰减)
    double w = vol_j / distsq;
    double dist = std::sqrt(distsq);

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
    double w = vols_[j] / std::max(distsq, 1e-12);

    P_surf += P_j * w;
    surf_w += w;
  }
  P_surf /= surf_w;

  // 3. Project slave i and get Gap (计算从面投影点与平滑曲面的垂向差距)
  double P_i = xi * nx + yi * ny + zi * nz;

  // NTS 抽象主面的等效体积安全限
  double avg_dx_j = 0;
  for (int j : masterNodes) {
    double distsq = (xi - coords_[j * 3]) * (xi - coords_[j * 3]) +
                    (yi - coords_[j * 3 + 1]) * (yi - coords_[j * 3 + 1]) +
                    (zi - coords_[j * 3 + 2]) * (zi - coords_[j * 3 + 2]);
    double w = vols_[j] / std::max(distsq, 1e-12);
    avg_dx_j += volToDx(vols_[j]) * w;
  }
  avg_dx_j /= surf_w;

  double safeDist = (dx_i + avg_dx_j) * 0.5 * pinballRatio_;
  double gap = P_i - P_surf;

  if (gap >= safeDist)
    return contexts; // 如果从面飘在虚拟平滑云的上方安全距离外，毫无穿透

  double penetration = safeDist - gap; // 穿刺了多少
  penetration =
      std::min(penetration, safeDist * 0.5); // 防止大步长穿心引发无穷大惩罚力

  // 4. Calculate smoothed master velocity array
  // (提取主面网格的随质点运动平滑速度)
  double dvx = 0, dvy = 0, dvz = 0;
  if (vel_) {
    double vx_master = 0, vy_master = 0, vz_master = 0;
    for (int j : masterNodes) {
      double distsq = (xi - coords_[j * 3]) * (xi - coords_[j * 3]) +
                      (yi - coords_[j * 3 + 1]) * (yi - coords_[j * 3 + 1]) +
                      (zi - coords_[j * 3 + 2]) * (zi - coords_[j * 3 + 2]);
      double w = vols_[j] / std::max(distsq, 1e-12);
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
  // Abstract mass representation
  double mass_master = (mat_master ? mat_master->getDensity() : 1.0) *
                       (avg_dx_j * avg_dx_j * avg_dx_j) * massScaleFactor_;

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
    double distsq = (xi - coords_[j * 3]) * (xi - coords_[j * 3]) +
                    (yi - coords_[j * 3 + 1]) * (yi - coords_[j * 3 + 1]) +
                    (zi - coords_[j * 3 + 2]) * (zi - coords_[j * 3 + 2]);
    double w = vols_[j] / std::max(distsq, 1e-12);
    cx.master_weights.push_back({j, w / surf_w});
  }

  contexts.push_back(cx);
  return contexts;
}

} // namespace PDCommon::Contact

REGISTER_EVALUATOR_TYPE(NTS, NTSEvaluator)
