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
  auto &fieldManager = ctx.getFieldManager();
  auto &manager = ctx.getParticleManager();
  const size_t numParticles = manager.getTotalParticles();

  std::vector<double> bulkArr = ExtractBulkModulusArray(manager);

  auto &reg = PDCommon::Field::FieldRegistry::getInstance();
  auto kdFieldNew = reg.createField("DoubleField", "KD_Tensor", 9);
  fieldManager.addField(std::move(kdFieldNew));
  auto *kdField = fieldManager.getFieldAs<double>("KD_Tensor");
  kdField->resize(numParticles);
  kdField->clearToZero();

  double *kdPtr = kdField->dataPtr();
  const double *shapeInvPtr =
      fieldManager.getFieldAs<double>("ShapeTensorInv")->dataPtr();

#pragma omp parallel for schedule(static)
  for (int i = 0; i < static_cast<int>(numParticles); ++i) {
    double k = bulkArr[i];

    int idx9 = i * 9;
    double i_k00 = shapeInvPtr[idx9], i_k01 = shapeInvPtr[idx9 + 1],
           i_k02 = shapeInvPtr[idx9 + 2];
    double i_k10 = shapeInvPtr[idx9 + 3], i_k11 = shapeInvPtr[idx9 + 4],
           i_k12 = shapeInvPtr[idx9 + 5];
    double i_k20 = shapeInvPtr[idx9 + 6], i_k21 = shapeInvPtr[idx9 + 7],
           i_k22 = shapeInvPtr[idx9 + 8];

    kdPtr[idx9] = k * (i_k00 * i_k00 + i_k01 * i_k10 + i_k02 * i_k20);
    kdPtr[idx9 + 1] = k * (i_k00 * i_k01 + i_k01 * i_k11 + i_k02 * i_k21);
    kdPtr[idx9 + 2] = k * (i_k00 * i_k02 + i_k01 * i_k12 + i_k02 * i_k22);

    kdPtr[idx9 + 3] = k * (i_k10 * i_k00 + i_k11 * i_k10 + i_k12 * i_k20);
    kdPtr[idx9 + 4] = k * (i_k10 * i_k01 + i_k11 * i_k11 + i_k12 * i_k21);
    kdPtr[idx9 + 5] = k * (i_k10 * i_k02 + i_k11 * i_k12 + i_k12 * i_k22);

    kdPtr[idx9 + 6] = k * (i_k20 * i_k00 + i_k21 * i_k10 + i_k22 * i_k20);
    kdPtr[idx9 + 7] = k * (i_k20 * i_k01 + i_k21 * i_k11 + i_k22 * i_k21);
    kdPtr[idx9 + 8] = k * (i_k20 * i_k02 + i_k21 * i_k12 + i_k22 * i_k22);
  }
  LOG_INFO("[MechanicalZhangStabilizer] Precomputed KD_Tensor globally.");
}

void MechanicalZhangStabilizer::applyPenalty(PDContext &ctx) {
  auto &fieldManager = ctx.getFieldManager();
  auto &neighborList = ctx.getNeighborList();
  auto &manager = ctx.getParticleManager();

  const size_t numParticles = manager.getTotalParticles();
  std::vector<double> rhoArr = ExtractDensityArray(manager);

  const double *coords = fieldManager.getFieldAs<double>("Coords")->dataPtr();
  const double *volumes = fieldManager.getFieldAs<double>("Volume")->dataPtr();
  const double *dispPtr =
      fieldManager.getFieldAs<double>("Displacement")->dataPtr();
  const double *F_Ptr =
      fieldManager.getFieldAs<double>("DeformationGradient")->dataPtr();
  const double *vvPtr = fieldManager.getFieldAs<double>("VHorizon")->dataPtr();
  const double *kdPtr =
      fieldManager.getFieldAs<double>("KD_Tensor")->dataPtr();
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
    int idx9_i = i * 9;
    double KD_00 = kdPtr[idx9_i], KD_01 = kdPtr[idx9_i + 1],
           KD_02 = kdPtr[idx9_i + 2];
    double KD_10 = kdPtr[idx9_i + 3], KD_11 = kdPtr[idx9_i + 4],
           KD_12 = kdPtr[idx9_i + 5];
    double KD_20 = kdPtr[idx9_i + 6], KD_21 = kdPtr[idx9_i + 7],
           KD_22 = kdPtr[idx9_i + 8];

    const double *Fi = &F_Ptr[idx9_i];
    double force_x = 0.0, force_y = 0.0, force_z = 0.0;

    const int numNeighbors = neighborList.getNeighborCount(i);
    const int *neighbors = neighborList.getNeighborIds(i);
    const int offset = neighbors - neighborList.getNeighborIds(0);

    for (int k_nb = 0; k_nb < numNeighbors; ++k_nb) {
      int j = neighbors[k_nb];
      if (j == -1)
        continue;

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
      ComputeNonAffineDisp(-du_x, -du_y, -du_z, -dx, -dy, -dz, Fj, z_jx, z_jy,
                           z_jz);

      // T_i 惩罚项
      double xi_KD_x = KD_00 * dx + KD_01 * dy + KD_02 * dz;
      double xi_KD_y = KD_10 * dx + KD_11 * dy + KD_12 * dz;
      double xi_KD_z = KD_20 * dx + KD_21 * dy + KD_22 * dz;
      double Z_i = omega * omega * (dx * xi_KD_x + dy * xi_KD_y + dz * xi_KD_z);
      double G_Zhang_i = Z_i * vj * vvPtr[j];

      // T_j 惩罚项
      int idx9_j = j * 9;
      double KD_j_00 = kdPtr[idx9_j], KD_j_01 = kdPtr[idx9_j + 1],
             KD_j_02 = kdPtr[idx9_j + 2];
      double KD_j_10 = kdPtr[idx9_j + 3], KD_j_11 = kdPtr[idx9_j + 4],
             KD_j_12 = kdPtr[idx9_j + 5];
      double KD_j_20 = kdPtr[idx9_j + 6], KD_j_21 = kdPtr[idx9_j + 7],
             KD_j_22 = kdPtr[idx9_j + 8];

      double j_xi_KD_x = KD_j_00 * (-dx) + KD_j_01 * (-dy) + KD_j_02 * (-dz);
      double j_xi_KD_y = KD_j_10 * (-dx) + KD_j_11 * (-dy) + KD_j_12 * (-dz);
      double j_xi_KD_z = KD_j_20 * (-dx) + KD_j_21 * (-dy) + KD_j_22 * (-dz);
      double Z_j =
          omega * omega * (dx * j_xi_KD_x + dy * j_xi_KD_y + dz * j_xi_KD_z);

      double G_Zhang_j = Z_j * vj * vv_i;

      force_x += g0_ * (G_Zhang_i * z_ix - G_Zhang_j * z_jx);
      force_y += g0_ * (G_Zhang_i * z_iy - G_Zhang_j * z_jy);
      force_z += g0_ * (G_Zhang_i * z_iz - G_Zhang_j * z_jz);
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
