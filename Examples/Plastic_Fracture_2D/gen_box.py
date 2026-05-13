# 生成简单的六面体盒子 STL
filename = "box.stl"

x_min, x_max = -25.0, 25.0
y_min, y_max = 0.0, 100.0
z_min, z_max = -0.5, 0.5

vertices = [
    [x_min, y_min, z_min], # 0
    [x_max, y_min, z_min], # 1
    [x_max, y_max, z_min], # 2
    [x_min, y_max, z_min], # 3
    [x_min, y_min, z_max], # 4
    [x_max, y_min, z_max], # 5
    [x_max, y_max, z_max], # 6
    [x_min, y_max, z_max]  # 7
]

# 逆时针朝外的三角形面片
faces = [
    # Bottom (-z)
    [0, 3, 2], [0, 2, 1],
    # Top (+z)
    [4, 5, 6], [4, 6, 7],
    # Front (-y)
    [0, 1, 5], [0, 5, 4],
    # Back (+y)
    [3, 7, 6], [3, 6, 2],
    # Left (-x)
    [0, 4, 7], [0, 7, 3],
    # Right (+x)
    [1, 2, 6], [1, 6, 5]
]

with open(filename, "w") as f:
    f.write("solid box\n")
    for face in faces:
        v1, v2, v3 = vertices[face[0]], vertices[face[1]], vertices[face[2]]
        f.write("facet normal 0 0 0\nouter loop\n")
        f.write(f"vertex {v1[0]} {v1[1]} {v1[2]}\n")
        f.write(f"vertex {v2[0]} {v2[1]} {v2[2]}\n")
        f.write(f"vertex {v3[0]} {v3[1]} {v3[2]}\n")
        f.write("endloop\nendfacet\n")
    f.write("endsolid box\n")
