import pyvista as pv

filename = r'D:\C++pro\GRPD\Examples\Axisymmetric_Ring\Result_20260518_090026\Axisymmetric_Ring_step00006_t0.3000.vtk'
mesh = pv.read(filename)
print("Mesh details:")
print(mesh)
print("\nPoint data arrays:")
for name in mesh.point_data.keys():
    print(f" - {name}: shape={mesh.point_data[name].shape}")
