import math
import matplotlib.pyplot as plt
import matplotlib.animation as animation
import numpy as np
from render import Renderer

rpm = 20

# Initialize the Renderer and get the render dictionary
renderer = Renderer(rpm=rpm)
render_dict = renderer.renderDict

# Precompute all LED positions and their activation times
points = []
period = 1/rpm * 60 # Rotation period in seconds (for 20 RPM)
max_radius = 64  # Maximum LED radius from the Renderer parameters

for time_ms in render_dict:
    time_sec = time_ms / 1000.0
    angle = (time_ms / 3000.0) * 2 * math.pi
    radii = render_dict[time_ms]
    
    for r in radii:
        x = r * math.cos(angle)
        y = r * math.sin(angle)
        points.append((x, y, time_sec))

# Sort points by activation time
points.sort(key=lambda p: p[2])

# Convert to numpy arrays for efficient slicing
x = np.array([p[0] for p in points])
y = np.array([p[1] for p in points])
times = np.array([p[2] for p in points])

# Set up the plot
fig, ax = plt.subplots(figsize=(8, 8))
ax.set_xlim(-max_radius-5, max_radius+5)
ax.set_ylim(-max_radius-5, max_radius+5)
ax.set_aspect('equal')
ax.set_title("Perspective-Correct LED Visualization")
scat = ax.scatter([], [], s=3, c='blue')

# Initialize the reference line
ref_line, = ax.plot([], [], c='red', alpha=0.5)

# Animation parameters
fps = 30
total_frames = int(fps * period)
interval = 1000 / fps  # ms per frame

def update(frame):
    current_time = (frame / fps) % period
    mask = times <= current_time
    
    # Update scatter plot with all points activated up to current_time
    scat.set_offsets(np.column_stack((x[mask], y[mask])))
    
    # Update the reference line
    ref_angle = (current_time / period) * 2 * math.pi
    ref_line.set_data([0, max_radius * math.cos(ref_angle)], 
                      [0, max_radius * math.sin(ref_angle)])
    
    return scat, ref_line

# Create and show the animation
ani = animation.FuncAnimation(
    fig, update, 
    frames=total_frames, 
    interval=interval, 
    blit=True
)

plt.show()