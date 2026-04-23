import math

f = open(r"d:\C++pro\GRPD\Examples\Plastic_Fracture_2D\cylinder.stl", "w")
f.write("solid dogbone\n")
segments = 36
length = 100.0

def get_r(y):
    # 基础半径10，中间（Y=50）略微缩小为8，强迫在中心发生颈缩
    return 10.0 - 2.0 * math.exp(-((y - 50.0) / 10.0)**2)

y_steps = 40
dy = length / y_steps

for j in range(y_steps):
    y1 = j * dy
    y2 = (j + 1) * dy
    r1 = get_r(y1)
    r2 = get_r(y2)
    for i in range(segments):
        theta1 = 2.0 * math.pi * i / segments
        theta2 = 2.0 * math.pi * (i + 1) / segments
        
        x11, z11 = r1 * math.cos(theta1), r1 * math.sin(theta1)
        x12, z12 = r1 * math.cos(theta2), r1 * math.sin(theta2)
        
        x21, z21 = r2 * math.cos(theta1), r2 * math.sin(theta1)
        x22, z22 = r2 * math.cos(theta2), r2 * math.sin(theta2)
        
        f.write("facet normal 0 0 0\nouter loop\n")
        f.write(f"vertex {x11} {y1} {z11}\nvertex {x12} {y1} {z12}\nvertex {x22} {y2} {z22}\nendloop\nendfacet\n")
        
        f.write("facet normal 0 0 0\nouter loop\n")
        f.write(f"vertex {x11} {y1} {z11}\nvertex {x22} {y2} {z22}\nvertex {x21} {y2} {z21}\nendloop\nendfacet\n")
        
        if j == 0:
            f.write("facet normal 0 -1 0\nouter loop\n")
            f.write(f"vertex {x12} {y1} {z12}\nvertex {x11} {y1} {z11}\nvertex 0.0 {y1} 0.0\nendloop\nendfacet\n")
        if j == y_steps - 1:
            f.write("facet normal 0 1 0\nouter loop\n")
            f.write(f"vertex {x21} {y2} {z21}\nvertex {x22} {y2} {z22}\nvertex 0.0 {y2} 0.0\nendloop\nendfacet\n")

f.write("endsolid dogbone\n")
f.close()
