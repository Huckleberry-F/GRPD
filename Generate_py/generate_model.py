#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import warnings
warnings.filterwarnings("ignore") # 屏蔽掉 numpy 1.26 在 Python 3.14 上的兼容性报警红字

import numpy as np
import os
import yaml
import argparse

def in_box(x, y, z, box):
    xmin, xmax, ymin, ymax, zmin, zmax = box
    return (x >= xmin) & (x <= xmax) & \
           (y >= ymin) & (y <= ymax) & \
           (z >= zmin) & (z <= zmax)

def _check_chunk_worker(mesh, i, chunk_points):
    """Worker function for parallel ray intersections."""
    return i, mesh.contains(chunk_points)

def generate_from_yaml(yaml_path):
    print("==================================================")
    print("[START] GRPD Multi-Part Assembly Pre-Processor...")
    
    with open(yaml_path, 'r', encoding='utf-8') as f:
        config = yaml.safe_load(f)
        
    try:
        parts = config['Parts']
    except KeyError as e:
        print(f"[FATAL] YAML missing required key: {e}")
        exit(1)
    
    # 自动推导输出文件名：与 YAML 所在文件夹同名 + .grpd
    yaml_dir = os.path.dirname(os.path.abspath(yaml_path))
    model_name = os.path.basename(yaml_dir)
    output_file = os.path.join(yaml_dir, model_name + ".grpd")
    print(f"[INFO]  Output grpd path: {output_file}")
        
    bc_entries = []
    
    # 1. 兼容原有的全局 BoundaryConditions (默认 Step = 0, 全时间通用)
    if 'BoundaryConditions' in config:
        for bc in config['BoundaryConditions']:
            bc_entries.append({'step': 0, 'bc': bc})
            
    # 2. 读取按步骤划分的 LoadStep_Conditions
    if 'LoadStep_Conditions' in config:
        for step_node in config['LoadStep_Conditions']:
            step_id = step_node.get('Step', 1)
            bcs = step_node.get('BoundaryConditions', [])
            for bc in bcs:
                bc_entries.append({'step': step_id, 'bc': bc})

    # 存储所有物质点数据的全局大列表
    global_particles = []
    global_id = 0 # 保证所有 Part 的粒子 ID 全局唯一连续
    
    # 1. 遍历生成所有 Part
    for part in parts:
        part_id = part['PartID']
        mat_id = part.get('MatID', 1)  # 默认无 MatID 则给 1
        dx = part['dx']
        offset = part.get('Offset', [0.0, 0.0, 0.0]) # 默认无平移
        
        print(f"[BUILD] Generating Part {part_id} with MatID {mat_id}...")
        
        if 'Source' in part:
            # ============================================================
            # STL/OBJ 体素化路径：从外部网格文件导入复杂几何
            # ============================================================
            try:
                import open3d as o3d
            except ImportError:
                print("[FATAL] 'open3d' package is required for STL import.")
                print("        Install it with: pip install open3d")
                exit(1)
            
            source_path = part['Source']
            # 支持相对于 YAML 文件的相对路径
            if not os.path.isabs(source_path):
                yaml_dir = os.path.dirname(os.path.abspath(yaml_path))
                source_path = os.path.join(yaml_dir, source_path)
            
            print(f"[STL]  Loading mesh from: {source_path}")
            mesh_o3d = o3d.io.read_triangle_mesh(source_path)
            
            if not mesh_o3d.has_vertices():
                print("[FATAL] Could not load mesh or mesh is empty.")
                exit(1)
                
            if not mesh_o3d.is_watertight():
                print("[REPAIR] Mesh is NOT watertight. Running safe cleanup...")
                
                # ---- 第一阶段：Open3D 安全拓扑清理（永远执行，不会破坏几何）----
                mesh_o3d.remove_degenerate_triangles()
                mesh_o3d.remove_duplicated_triangles()
                mesh_o3d.remove_duplicated_vertices()
                mesh_o3d.remove_non_manifold_edges()
                
                if mesh_o3d.is_watertight():
                    print("[REPAIR] Stage 1 (Open3D cleanup) — OK!")
                else:
                    # ---- 第二阶段：PyMeshFix 深度修复（仅当 YAML 指定 RepairMesh: true 时执行）----
                    repair_flag = part.get('RepairMesh', False)
                    if repair_flag:
                        print("[REPAIR] Stage 1 insufficient. RepairMesh=true, invoking PyMeshFix...")
                        try:
                            from pymeshfix._meshfix import clean_from_arrays
                        except ImportError:
                            import subprocess, sys
                            subprocess.check_call([sys.executable, "-m", "pip", "install", "pymeshfix", "-q"])
                            from pymeshfix._meshfix import clean_from_arrays
                        
                        try:
                            verts = np.asarray(mesh_o3d.vertices, dtype=np.float64)
                            faces = np.asarray(mesh_o3d.triangles, dtype=np.int32)
                            fixed_verts, fixed_faces = clean_from_arrays(verts, faces, verbose=False)
                            mesh_o3d.vertices = o3d.utility.Vector3dVector(fixed_verts)
                            mesh_o3d.triangles = o3d.utility.Vector3iVector(fixed_faces)
                            mesh_o3d.compute_vertex_normals()
                            
                            if mesh_o3d.is_watertight():
                                print("[REPAIR] Stage 2 (PyMeshFix) — Mesh repaired to watertight!")
                            else:
                                print("[WARNING] Mesh still not perfectly watertight after all repairs.")
                        except Exception as e:
                            print(f"[WARNING] PyMeshFix repair failed: {e}")
                    else:
                        print("[INFO]  Mesh not watertight, but RepairMesh not enabled.")
                        print("        Proceeding with occupancy check (usually works fine).")
                        print("        To enable deep repair, add 'RepairMesh: true' to this Part.")
            
            # 缩放支持 (可选)
            scale = part.get('Scale', 1.0)
            if scale != 1.0:
                # Open3D 默认按照指定中心缩放，这里保持与 Trimesh 相同的基于原点缩放
                mesh_o3d.scale(scale, center=(0, 0, 0))
                print(f"[STL]  Applied scale factor: {scale}")
            
            # 旋转支持 (可选，绕XYZ轴的角度，单位：度)
            rotation = part.get('Rotate', [0.0, 0.0, 0.0])
            if rotation != [0.0, 0.0, 0.0]:
                R = mesh_o3d.get_rotation_matrix_from_xyz((
                    np.radians(rotation[0]), 
                    np.radians(rotation[1]), 
                    np.radians(rotation[2])
                ))
                mesh_o3d.rotate(R, center=(0, 0, 0))
                print(f"[STL]  Applied rotation: {rotation} degrees")
                
            # 平移支持 (可选)
            translate = part.get('Translate', [0.0, 0.0, 0.0])
            if translate != [0.0, 0.0, 0.0]:
                mesh_o3d.translate(translate)
                print(f"[STL]  Applied translation: {translate}")

            
            # 在包围盒内生成均匀网格点
            bbox = mesh_o3d.get_axis_aligned_bounding_box()
            bbox_min = bbox.get_min_bound()
            bbox_max = bbox.get_max_bound()
            print(f"[STL]  Bounding box: ({bbox_min[0]:.2f}, {bbox_min[1]:.2f}, {bbox_min[2]:.2f})"
                  f" -> ({bbox_max[0]:.2f}, {bbox_max[1]:.2f}, {bbox_max[2]:.2f})")
            
            dim = part.get('Dimension', 3)
            
            x_coords = np.arange(bbox_min[0] + dx/2, bbox_max[0], dx)
            y_coords = np.arange(bbox_min[1] + dx/2, bbox_max[1], dx)
            
            # 先构建 2D XY 平面网格，避免 3D 网格导致内存爆炸 (O(N^3) -> O(N^2))
            X_2d, Y_2d = np.meshgrid(x_coords, y_coords, indexing='ij')
            x_flat_2d = X_2d.ravel()
            y_flat_2d = Y_2d.ravel()
            pts_per_slice = len(x_flat_2d)
            
            if dim == 2:
                z_coords = np.array([(bbox_min[2] + bbox_max[2]) / 2.0])
                thickness = part.get('Thickness', bbox_max[2] - bbox_min[2])
                volume = dx * dx * thickness
            else:
                z_coords = np.arange(bbox_min[2] + dx/2, bbox_max[2], dx)
                volume = dx * dx * dx
                
            total_slices = len(z_coords)
            total_candidates = total_slices * pts_per_slice
            print(f"[STL]  Total candidate points in bounding box: {total_candidates} ({total_slices} Z-slices)")
            
            # 使用分批生成点来做射线碰撞检测，极其节省内存！
            import time
            start_time = time.time()
            valid_x, valid_y, valid_z = [], [], []
            
            # 准备待处理的 Z 层数据列表，按小 block 打包
            slice_blocks = []
            block_size = max(1, 200000 // max(1, pts_per_slice)) # 每个 block 约 20 万个候选点
            
            for i in range(0, total_slices, block_size):
                end_i = min(i + block_size, total_slices)
                # 为该 block 生成所有 3D 点
                block_pts = []
                for z_idx in range(i, end_i):
                    z_val = z_coords[z_idx]
                    pts = np.column_stack([x_flat_2d, y_flat_2d, np.full(pts_per_slice, z_val)])
                    block_pts.append(pts)
                
                block_pts_array = np.vstack(block_pts)
                slice_blocks.append((i, block_pts_array))
            
            print(f"[STL]  Converting mesh to Open3D RaycastingScene for extreme acceleration...")
            
            # 将旧版 TriangleMesh 转为用于高速射线追踪的 Tensor 格式
            mesh_t = o3d.t.geometry.TriangleMesh.from_legacy(mesh_o3d)
            
            scene = o3d.t.geometry.RaycastingScene()
            _ = scene.add_triangles(mesh_t)

            print(f"[STL]  Checking ray intersections in {len(slice_blocks)} memory-friendly blocks with Open3D...")
            
            total_inside = 0
            completed = 0
            
            for b_idx, pts in slice_blocks:
                pts_tensor = o3d.core.Tensor(pts, dtype=o3d.core.Dtype.Float32)
                occupancy = scene.compute_occupancy(pts_tensor)
                in_mask = occupancy.numpy() > 0.5
                
                inside = pts[in_mask]
                
                if len(inside) > 0:
                    valid_x.append(inside[:, 0])
                    valid_y.append(inside[:, 1])
                    valid_z.append(inside[:, 2])
                    total_inside += len(inside)
                    
                completed += 1
                elapsed = time.time() - start_time
                print(f"[STL]    Progress: {completed}/{len(slice_blocks)} blocks ({(completed/len(slice_blocks))*100:.1f}%) - Found: {total_inside} pts - Elapsed: {elapsed:.1f}s", flush=True)
            
            print(f"[STL]  Points inside mesh: {total_inside} / {total_candidates} ({100*total_inside/max(total_candidates,1):.1f}%)")
            
            # 合并有效点
            if total_inside > 0:
                x_flat = np.concatenate(valid_x)
                y_flat = np.concatenate(valid_y)
                z_flat = np.concatenate(valid_z) if dim == 3 else np.zeros(total_inside)
                
                # ===========================================================
                # 孤立粒子过滤：移除邻居数不足的飘散粒子
                # 使用 Open3D KDTree 统计每个点在 1.5*dx 范围内的邻居数，
                # 邻居数 < 4 的视为孤立粒子并剔除
                # ===========================================================
                pts_arr = np.column_stack([x_flat, y_flat, z_flat])
                pcd_filter = o3d.geometry.PointCloud()
                pcd_filter.points = o3d.utility.Vector3dVector(pts_arr)
                kdtree = o3d.geometry.KDTreeFlann(pcd_filter)
                
                min_neighbors = 4 if dim == 3 else 3
                search_radius = dx * 1.5
                keep_mask = np.ones(len(pts_arr), dtype=bool)
                
                for idx in range(len(pts_arr)):
                    [count, _, _] = kdtree.search_radius_vector_3d(pcd_filter.points[idx], search_radius)
                    # count 包含自身，所以实际邻居数 = count - 1
                    if (count - 1) < min_neighbors:
                        keep_mask[idx] = False
                
                n_removed = np.sum(~keep_mask)
                if n_removed > 0:
                    print(f"[FILTER] Removed {n_removed} isolated particles (neighbors < {min_neighbors} within {search_radius:.4f})")
                    x_flat = x_flat[keep_mask]
                    y_flat = y_flat[keep_mask]
                    z_flat = z_flat[keep_mask]
                    total_inside = len(x_flat)
                    print(f"[FILTER] Remaining points: {total_inside}")
            else:
                x_flat = np.array([])
                y_flat = np.array([])
                z_flat = np.array([])
            
        else:
            # ============================================================
            # 原始矩形网格路径：Length x Width x Thickness + dx
            # ============================================================
            dim = part['Dimension']
            
            x_coords = np.arange(dx / 2, part['Length'], dx)
            y_coords = np.arange(dx / 2, part['Width'],  dx)
            
            if dim == 2:
                X, Y = np.meshgrid(x_coords, y_coords, indexing='ij')
                x_flat, y_flat = X.ravel(), Y.ravel()
                z_flat = np.zeros_like(x_flat)
                volume = dx * dx * part['Thickness']
            else:
                z_coords = np.arange(dx / 2, part['Thickness'], dx)
                X, Y, Z = np.meshgrid(x_coords, y_coords, z_coords, indexing='ij')
                x_flat, y_flat, z_flat = X.ravel(), Y.ravel(), Z.ravel()
                volume = dx * dx * dx

        # 应用平移偏移量 (非常关键，组合装配体必备)
        x_flat = x_flat + offset[0]
        y_flat = y_flat + offset[1]
        z_flat = z_flat + offset[2]

        num_part_particles = len(x_flat)
        
        # [NEW] 矢量化计算单 Part 内多材料区域
        mat_flat = np.full(num_part_particles, mat_id, dtype=int)
        if 'MatRegions' in part:
            for region in part['MatRegions']:
                box = region['Box']
                r_mat = region.get('MatID', mat_id)
                mask = in_box(x_flat, y_flat, z_flat, box)
                mat_flat[mask] = r_mat

        # 将当前 Part 的点推入全局数组
        for i in range(num_part_particles):
            global_particles.append({
                'ID': global_id,
                'PartID': part_id,
                'MatID': mat_flat[i],
                'X': x_flat[i],
                'Y': y_flat[i],
                'Z': z_flat[i],
                'Volume': volume
            })
            global_id += 1

    total_particles = len(global_particles)
    
    # 将全局数据转化为 numpy 数组，方便矢量化计算边界
    all_x = np.array([p['X'] for p in global_particles])
    all_y = np.array([p['Y'] for p in global_particles])
    all_z = np.array([p['Z'] for p in global_particles])
    all_ids = np.array([p['ID'] for p in global_particles])

    # 2. 写入极其工整的 .grpd 文件
    output_dir = os.path.dirname(output_file)
    if output_dir:
        os.makedirs(output_dir, exist_ok=True)
    with open(output_file, 'w', encoding='utf-8') as f:
        
        # --- 写入几何块 ---
        f.write("*PARTICLE\n")
        # HPC 前置预分配：向 C++ 引擎提前通告总粒子数，实现 Zero-Reallocation
        f.write(f"TotalParticles,{total_particles}\n")
        # 表头也对其占位，方便阅读
        f.write(f"#{'ID':>19}, {'PartID':>19}, {'MatID':>19}, {'X':>19}, {'Y':>19}, {'Z':>19}, {'Volume':>19}\n")
        
        for p in global_particles:
            # 魔法格式化： >20 表示右对齐并占满20个字符； .6f 表示保留6位小数的浮点数
            # 注意：逗号算在外部，所以单个数字绝对占20字符
            line = f"{p['ID']:>20},{p['PartID']:>20},{p['MatID']:>20},{p['X']:>20.6f},{p['Y']:>20.6f},{p['Z']:>20.6f},{p['Volume']:>20.8f}\n"
            f.write(line)
            
        # --- 写入边界条件块 (ANSYS APDL 单自由度格式) ---
        f.write("*LOAD\n")
        f.write(f"#{'ID':>19}, {'Step':>19}, {'BcID':>19}, {'DOF':>19}, {'Value':>19}\n")
        
        # DOF 类型映射表：从旧的组合类型 + 轴索引映射到 APDL 标签
        _DOF_MAP = {
            'DISP': ['UX', 'UY', 'UZ'],
            'VELOCITY': ['VX', 'VY', 'VZ'],
            'BODY_FORCE': ['AX', 'AY', 'AZ'],
            'PRESSURE': ['PX', 'PY', 'PZ'],
        }
        
        bc_count = 0
        for entry in bc_entries:
            step_id = entry['step']
            bc = entry['bc']
            
            mask = in_box(all_x, all_y, all_z, bc['Box'])
            if np.sum(mask) == 0:
                continue
            
            bc_id = bc.get('BcID', 0)
            bc_type = bc['Type']
            val_data = bc['Value']
            apply_dir = bc.get('ApplyDir', None)
            
            # ============================================================
            # ANSYS APDL 单自由度平铺爆炸
            # ============================================================
            
            # 情况 1：已经是单自由度标签 (UX, UY, VZ, T, FLUX, CONV 等)
            if bc_type not in _DOF_MAP:
                # 直接写入，值可以是标量或列表
                if isinstance(val_data, list):
                    val_str = ",".join([f"{float(v):>20.6f}" if not isinstance(v, str) else f"{v:>20}" for v in val_data])
                else:
                    val_str = f"{float(val_data):>20.6f}" if not isinstance(val_data, str) else f"{val_data:>20}"
                
                for i in range(total_particles):
                    if mask[i]:
                        line = f"{all_ids[i]:>20},{step_id:>20},{bc_id:>20},{bc_type:>20},{val_str}\n"
                        f.write(line)
                        bc_count += 1
            else:
                # 情况 2：旧式组合标签 (DISP, VELOCITY 等)，需要爆炸为单 DOF 行
                dof_labels = _DOF_MAP[bc_type]
                
                # 确保 val_data 是列表
                if not isinstance(val_data, list):
                    val_data = [val_data]
                
                # 确定每个方向是否生效
                for axis in range(min(len(val_data), 3)):
                    # 检查 ApplyDir：如果显式声明了，按它来决定
                    if apply_dir is not None:
                        if axis < len(apply_dir) and not apply_dir[axis]:
                            continue  # 该方向不约束，直接跳过，不生成行
                    
                    dof_label = dof_labels[axis]
                    val = val_data[axis]
                    
                    if isinstance(val, str):
                        val_str = f"{val:>20}"
                    else:
                        val_str = f"{float(val):>20.6f}"
                    
                    for i in range(total_particles):
                        if mask[i]:
                            line = f"{all_ids[i]:>20},{step_id:>20},{bc_id:>20},{dof_label:>20},{val_str}\n"
                            f.write(line)
                            bc_count += 1

    print(f"[DONE] Assembly complete! Parts: {len(parts)}, Total particles: {total_particles}")
    print(f"[DONE] Mapped {bc_count} boundary condition data points.")
    print(f"[DONE] Saved to: {output_file}")
    print("==================================================\n")

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("config", type=str, nargs='?', default="config.yaml")
    args = parser.parse_args()
    generate_from_yaml(args.config)