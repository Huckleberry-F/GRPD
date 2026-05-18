import numpy as np
import os

# --- DENT (Double Edge Notched Tension) 试样参数 ---
L = 100.0  # 总长度 Y (-50 到 50)
W = 20.0   # 总宽度 X (-10 到 10)
R = 4.0    # 两侧半圆缺口半径 (缺口深度 a=4)
dx = 0.5   # 粒子间距 (网格密度)
thickness = 1.0 # 2D 等效厚度

points = []
volumes = []

x_range = np.arange(-W/2 + dx/2, W/2, dx)
y_range = np.arange(-L/2 + dx/2, L/2, dx)

for y in y_range:
    for x in x_range:
        # 左侧缺口判定，圆心在 (-10, 0)
        dist_left = np.sqrt((x - (-W/2))**2 + y**2)
        # 右侧缺口判定，圆心在 (10, 0)
        dist_right = np.sqrt((x - (W/2))**2 + y**2)
        
        # 只有在缺口圆弧以外的点才被保留
        if dist_left > R and dist_right > R:
            points.append([x, y, 0.0])
            volumes.append(dx * dx * thickness)

out_file = os.path.join(os.path.dirname(__file__), "DENT.txt")
with open(out_file, "w") as f:
    for i, p in enumerate(points):
        f.write(f"{p[0]:.4f} {p[1]:.4f} {p[2]:.4f} {volumes[i]:.4f}\n")

print(f"Success! Generated {len(points)} particles for DENT specimen.")
print(f"Saved to: {out_file}")
