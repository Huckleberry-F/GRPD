import sys
import os

file_path = r'd:\Project_C++\GRPD\PDCommon\Kernel\src\MechanicalStabilizers.cpp'
with open(file_path, 'r', encoding='utf-8') as f:
    content = f.read()

start_marker = '// =========================================================================\n// MechanicalZhangStabilizer 实现\n// ========================================================================='
start_idx = content.find(start_marker)

end_marker = '} // namespace PDCommon::Kernel'
end_idx = content.find(end_marker, start_idx)

new_code = """
void MechanicalZhangStabilizer::preCompute(PDContext &ctx) {
  // 张量二次型的实时构造替代了全局存储，废弃张量缓存逻辑以释放显存
}

void MechanicalZhangStabilizer::applyPenalty(PDContext &ctx) {
  auto &fieldManager = ctx.getFieldManager();
  auto &neighborList = ctx.getNeighborList();
  auto &manager = ctx.getParticleManager();

  const size_t numParticles = manager.getTotalParticles();
  std::vector<double> lambdaArr, muArr;
  ExtractLameParameters(manager, lambdaArr, muArr);
  std::vector<double> rhoArr = ExtractDensityArray(manager);

  const double *coords = fieldManager.getFieldAs<double>("Coords")->dataPtr();
  const double *volumes = fieldManager.getFieldAs<double>("Volume")->dataPtr();
  const double *dispPtr = fieldManager.getFieldAs<double>("Displacement")->dataPtr();
  const double *F_Ptr = fieldManager.getFieldAs<double>("DeformationGradient")->dataPtr();
  const double *vvPtr = fieldManager.getFieldAs<double>("VHorizon")->dataPtr();
  const double *shapeInvPtr = fieldManager.getFieldAs<double>("ShapeTensorInv")->dataPtr();
  const double *omegaPtr = neighborList.getBondFieldPtr("InfluenceWeight");
  double *accPtr = fieldManager.getFieldAs<double>("Acceleration")->dataPtr();

#pragma omp parallel for schedule(static)
  for (int i = 0; i < static_cast<int>(numParticles); ++i) {
    double xi_x = coords[i * 3];
    double xi_y = coords[i * 3 + 1];
    double xi_z = coords[i * 3 + 2];
    double u_ix = dispPtr[i * 3];
    double u_iy = dispPtr[i * 3 + 1];
    double u_iz = dispPtr[i * 3 + 2];

    double vv_i = vvPtr[i];
    double lam_i = lambdaArr[i];
    double mu_i = muArr[i];
    int idx9_i = i * 9;
    double i_k00 = shapeInvPtr[idx9_i], i_k01 = shapeInvPtr[idx9_i + 1], i_k02 = shapeInvPtr[idx9_i + 2];
    double i_k10 = shapeInvPtr[idx9_i + 3], i_k11 = shapeInvPtr[idx9_i + 4], i_k12 = shapeInvPtr[idx9_i + 5];
    double i_k20 = shapeInvPtr[idx9_i + 6], i_k21 = shapeInvPtr[idx9_i + 7], i_k22 = shapeInvPtr[idx9_i + 8];

    const double *Fi = &F_Ptr[idx9_i];
    double force_x = 0.0, force_y = 0.0, force_z = 0.0;

    const int numNeighbors = neighborList.getNeighborCount(i);
    const int *neighbors = neighborList.getNeighborIds(i);
    const int offset = neighbors - neighborList.getNeighborIds(0);

    for (int k_nb = 0; k_nb < numNeighbors; ++k_nb) {
      int j = neighbors[k_nb];
      if (j == -1) continue;

      double dx = coords[j * 3] - xi_x;
      double dy = coords[j * 3 + 1] - xi_y;
      double dz = coords[j * 3 + 2] - xi_z;

      double du_x = dispPtr[j * 3] - u_ix;
      double du_y = dispPtr[j * 3 + 1] - u_iy;
      double du_z = dispPtr[j * 3 + 2] - u_iz;

      double vj = volumes[j];
      double omega = omegaPtr[offset + k_nb];

      double z_ix, z_iy, z_iz;
      ComputeNonAffineDisp(du_x, du_y, du_z, dx, dy, dz, Fi, z_ix, z_iy, z_iz);

      const double *Fj = &F_Ptr[j * 9];
      double z_jx, z_jy, z_jz;
      ComputeNonAffineDisp(-du_x, -du_y, -du_z, -dx, -dy, -dz, Fj, z_jx, z_jy, z_jz);

      // p_i = K^{-1}_i * xi
      double px_i = i_k00 * dx + i_k01 * dy + i_k02 * dz;
      double py_i = i_k10 * dx + i_k11 * dy + i_k12 * dz;
      double pz_i = i_k20 * dx + i_k21 * dy + i_k22 * dz;
      double p_dot_p_i = px_i * px_i + py_i * py_i + pz_i * pz_i;
      double p_dot_z_i = px_i * z_ix + py_i * z_iy + pz_i * z_iz;

      double o2 = omega * omega;
      double lam_mu_i = lam_i + mu_i;
      double vv_j = vvPtr[j];
      double t_ix = o2 * (lam_mu_i * p_dot_z_i * px_i + mu_i * p_dot_p_i * z_ix) * vv_j;
      double t_iy = o2 * (lam_mu_i * p_dot_z_i * py_i + mu_i * p_dot_p_i * z_iy) * vv_j;
      double t_iz = o2 * (lam_mu_i * p_dot_z_i * pz_i + mu_i * p_dot_p_i * z_iz) * vv_j;

      // j 的项 (p_j = K^{-1}_j * xi_j, 但是 xi_j = X_i - X_j = -dx)
      int idx9_j = j * 9;
      double j_k00 = shapeInvPtr[idx9_j], j_k01 = shapeInvPtr[idx9_j + 1], j_k02 = shapeInvPtr[idx9_j + 2];
      double j_k10 = shapeInvPtr[idx9_j + 3], j_k11 = shapeInvPtr[idx9_j + 4], j_k12 = shapeInvPtr[idx9_j + 5];
      double j_k20 = shapeInvPtr[idx9_j + 6], j_k21 = shapeInvPtr[idx9_j + 7], j_k22 = shapeInvPtr[idx9_j + 8];

      double px_j = j_k00 * (-dx) + j_k01 * (-dy) + j_k02 * (-dz);
      double py_j = j_k10 * (-dx) + j_k11 * (-dy) + j_k12 * (-dz);
      double pz_j = j_k20 * (-dx) + j_k21 * (-dy) + j_k22 * (-dz);
      double p_dot_p_j = px_j * px_j + py_j * py_j + pz_j * pz_j;
      double p_dot_z_j = px_j * z_jx + py_j * z_jy + pz_j * z_jz;

      double lam_j = lambdaArr[j];
      double mu_j = muArr[j];
      double lam_mu_j = lam_j + mu_j;
      double t_jx = o2 * (lam_mu_j * p_dot_z_j * px_j + mu_j * p_dot_p_j * z_jx) * vv_i;
      double t_jy = o2 * (lam_mu_j * p_dot_z_j * py_j + mu_j * p_dot_p_j * z_jy) * vv_i;
      double t_jz = o2 * (lam_mu_j * p_dot_z_j * pz_j + mu_j * p_dot_p_j * z_jz) * vv_i;

      force_x += g0_ * (t_ix - t_jx) * vj;
      force_y += g0_ * (t_iy - t_jy) * vj;
      force_z += g0_ * (t_iz - t_jz) * vj;
    }

    if (rhoArr[i] > 0.0) {
      accPtr[i * 3] += force_x / rhoArr[i];
      accPtr[i * 3 + 1] += force_y / rhoArr[i];
      accPtr[i * 3 + 2] += force_z / rhoArr[i];
    }
  }
}

"""

new_content = content[:start_idx + len(start_marker)] + "\n" + new_code + content[end_idx:]

with open(file_path, 'w', encoding='utf-8') as f:
    f.write(new_content)
