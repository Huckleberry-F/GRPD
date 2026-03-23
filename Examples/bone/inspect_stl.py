#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
inspect_stl.py — 检查 STL 文件尺寸并推荐 dx 和 YAML 配置
用法: python inspect_stl.py 你的文件.stl
"""

import sys
import open3d as o3d
import numpy as np

def inspect(filepath):
    print(f"\n=== 加载 STL: {filepath} ===")
    mesh = o3d.io.read_triangle_mesh(filepath)

    bbox = mesh.get_axis_aligned_bounding_box()
    bbox_min, bbox_max = bbox.get_min_bound(), bbox.get_max_bound()
    size = bbox_max - bbox_min

    print(f"\n【几何信息】")
    print(f"  包围盒最小点: ({bbox_min[0]:.3f}, {bbox_min[1]:.3f}, {bbox_min[2]:.3f})")
    print(f"  包围盒最大点: ({bbox_max[0]:.3f}, {bbox_max[1]:.3f}, {bbox_max[2]:.3f})")
    print(f"  X 方向尺寸:  {size[0]:.3f}")
    print(f"  Y 方向尺寸:  {size[1]:.3f}")
    print(f"  Z 方向尺寸:  {size[2]:.3f}")
    print(f"  最短边:      {min(size):.3f}")
    print(f"  最长边:      {max(size):.3f}")
    print(f"  是否水密:    {'✅ 是' if mesh.is_watertight() else '❌ 否 (可能影响体素化精度)'}")
    print(f"  面片数:      {len(mesh.triangles)}")

    # 推荐 dx（最短边的 1/20 ~ 1/50）
    short = min(size)
    dx_coarse = round(short / 20, 3)   # 粗网格 (快速测试)
    dx_medium = round(short / 50, 3)   # 中等精度
    dx_fine   = round(short / 100, 3)  # 精细
    
    # 估算粒子数
    if mesh.is_watertight():
        vol = mesh.get_volume()
    else:
        # 极端非水密模型连凸包都有可能由于精度报错非水密，因此直接用包围盒体积粗略折算（近似50%占比）来估算粒子数
        vol = size[0] * size[1] * size[2] * 0.5

    def est_particles(dx):
        return int(vol / (dx**3))

    print(f"\n【推荐 dx 参考】")
    print(f"  粗网格 (快速测试): dx = {dx_coarse}  → 约 {est_particles(dx_coarse):,} 粒子")
    print(f"  中等精度:          dx = {dx_medium}  → 约 {est_particles(dx_medium):,} 粒子")
    print(f"  精细:              dx = {dx_fine}  → 约 {est_particles(dx_fine):,} 粒子")

    print(f"\n【推荐 YAML 配置 (3D)】")
    print(f"""
Parts:
  - PartID: 1
    MatID: 1
    Source: "{filepath.replace(chr(92), '/')}"
    dx: {dx_coarse}    # 粗网格，先跑通再细化
    Offset: [0.0, 0.0, 0.0]

Solver:
  Horizon: {dx_coarse * 3.015:.3f}    # 建议 3 * dx
""")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("用法: python inspect_stl.py 你的文件.stl")
        sys.exit(1)
    inspect(sys.argv[1])
