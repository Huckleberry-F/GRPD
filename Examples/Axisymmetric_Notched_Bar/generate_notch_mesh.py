import numpy as np
import os

def generate_mesh():
    # Geometry parameters
    R_max = 9.0  # mm
    H_max = 20.0 # mm
    H_min = -20.0 # mm (Mirrored to the bottom)
    Notch_R = 4.0 # mm
    Notch_center = (9.0, 0.0)
    
    # Discretization
    dx = 0.1 # mm, adjust for density
    
    # Generate grid
    x_coords = np.arange(dx/2, R_max, dx)
    y_coords = np.arange(H_min + dx/2, H_max, dx)
    
    xx, yy = np.meshgrid(x_coords, y_coords)
    
    x_flat = xx.flatten()
    y_flat = yy.flatten()
    
    # Filter out points inside the notch
    # Distance to (9.0, 0.0) <= 4.0
    dist_sq = (x_flat - Notch_center[0])**2 + (y_flat - Notch_center[1])**2
    mask = dist_sq > Notch_R**2
    
    x_keep = x_flat[mask]
    y_keep = y_flat[mask]
    z_keep = np.zeros_like(x_keep)
    
    num_particles = len(x_keep)
    volume = dx * dx # 2D Area
    
    out_file = 'notched_bar_mesh.txt'
    with open(out_file, 'w') as f:
        # Don't write the number of particles on the first line to be compatible with generate_model.py
        for i in range(num_particles):
            f.write(f"{x_keep[i]:.6f} {y_keep[i]:.6f} {z_keep[i]:.6f} {volume:.6e}\n")
            
    print(f"Generated {num_particles} particles. Saved to {out_file}")
    print(f"Volume (Area) = {volume:.6e}")
    print(f"Spacing = {dx}")

if __name__ == '__main__':
    generate_mesh()
