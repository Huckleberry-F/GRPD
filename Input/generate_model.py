#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import numpy as np
import os
import yaml
import argparse

def in_box(x, y, z, box):
    xmin, xmax, ymin, ymax, zmin, zmax = box
    return (x >= xmin) & (x <= xmax) & \
           (y >= ymin) & (y <= ymax) & \
           (z >= zmin) & (z <= zmax)

def generate_from_yaml(yaml_path):
    print("==================================================")
    print("[START] GRPD Multi-Part Assembly Pre-Processor...")
    
    with open(yaml_path, 'r', encoding='utf-8') as f:
        config = yaml.safe_load(f)
        
    try:
        output_file = config['Assembly']['OutputGrpd']
        parts = config['Parts']
    except KeyError as e:
        print(f"[FATAL] YAML missing required key: {e}")
        exit(1)
        
    bc_configs = config.get('BoundaryConditions', [])

    # 存储所有物质点数据的全局大列表
    global_particles = []
    global_id = 0 # 保证所有 Part 的粒子 ID 全局唯一连续
    
    # 1. 遍历生成所有 Part
    for part in parts:
        part_id = part['PartID']
        mat_id = part.get('MatID', 1)  # 默认无 MatID 则给 1
        dim = part['Dimension']
        dx = part['dx']
        offset = part.get('Offset', [0.0, 0.0, 0.0]) # 默认无平移
        
        print(f"[BUILD] Generating Part {part_id} with MatID {mat_id}...")
        
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
        x_flat += offset[0]
        y_flat += offset[1]
        z_flat += offset[2]

        num_part_particles = len(x_flat)
        
        # 将当前 Part 的点推入全局数组
        for i in range(num_part_particles):
            global_particles.append({
                'ID': global_id,
                'PartID': part_id,
                'MatID': mat_id,
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
    os.makedirs(os.path.dirname(output_file), exist_ok=True)
    with open(output_file, 'w', encoding='utf-8') as f:
        
        # --- 写入几何块 ---
        f.write("*PARTICLE\n")
        # 表头也对其占位，方便阅读
        f.write(f"#{'ID':>19}, {'PartID':>19}, {'MatID':>19}, {'X':>19}, {'Y':>19}, {'Z':>19}, {'Volume':>19}\n")
        
        for p in global_particles:
            # 魔法格式化： >20 表示右对齐并占满20个字符； .6f 表示保留6位小数的浮点数
            # 注意：逗号算在外部，所以单个数字绝对占20字符
            line = f"{p['ID']:>20},{p['PartID']:>20},{p['MatID']:>20},{p['X']:>20.6f},{p['Y']:>20.6f},{p['Z']:>20.6f},{p['Volume']:>20.8f}\n"
            f.write(line)
            
        # --- 写入边界条件块 ---
        f.write("*LOAD\n")
        f.write(f"#{'ID':>19}, {'BcID':>19}, {'Type':>19}, {'Value(s)...':>19}\n")
        
        bc_count = 0
        for bc in bc_configs:
            mask = in_box(all_x, all_y, all_z, bc['Box'])
            if np.sum(mask) == 0:
                continue
            
            bc_id = bc.get('BcID', 0)
            bc_type = bc['Type']
            val_data = bc['Value'] 
            
            # 核心升级：智能处理单参数与多参数
            if isinstance(val_data, list):
                val_str = ",".join([f"{v:>20.6f}" for v in val_data])
            else:
                val_str = f"{val_data:>20.6f}"
            
            for i in range(total_particles):
                if mask[i]:
                    line = f"{all_ids[i]:>20},{bc_id:>20},{bc_type:>20},{val_str}\n"
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