import numpy as np

def generate_ring_mesh():
    # Geometry parameters
    R_in = 0.5   # Inner radius (X_min)
    R_out = 0.6  # Outer radius (X_max)
    H = 1.0      # Height (Y_max, Y_min = 0)
    
    # Discretization
    spacing = 0.01
    
    # Generate grid
    x_coords = np.arange(R_in + spacing/2, R_out, spacing)
    y_coords = np.arange(spacing/2, H, spacing)
    
    xx, yy = np.meshgrid(x_coords, y_coords)
    
    # Flatten
    coords_x = xx.flatten()
    coords_y = yy.flatten()
    coords_z = np.zeros_like(coords_x)
    
    num_particles = len(coords_x)
    volume = spacing * spacing # 2D Area
    
    with open('ring_mesh.txt', 'w') as f:
        f.write(f"{num_particles}\n")
        for i in range(num_particles):
            f.write(f"{coords_x[i]:.6f} {coords_y[i]:.6f} {coords_z[i]:.6f} {volume:.6e}\n")
            
    print(f"Generated {num_particles} particles. Saved to ring_mesh.txt")
    print(f"Volume (Area) = {volume:.6e}")
    print(f"Spacing = {spacing}")

if __name__ == '__main__':
    generate_ring_mesh()
