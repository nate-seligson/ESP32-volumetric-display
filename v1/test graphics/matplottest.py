import matplotlib.pyplot as plt
import numpy as np
import math
from render import Renderer
from matplotlib.animation import FuncAnimation
rpm = 100
# Initialize renderer with the correct RPM (assuming Renderer uses RPM correctly)
renderer = Renderer(rpm=rpm)
# Set up the plot
fig, ax = plt.subplots()
ax.set_xlim(-64, 64)
ax.set_ylim(-64, 64)
ax.set_title("Spinning Line with Stationary Dots")
line, = ax.plot([], [], lw=2)

line_length = 128

# Create a scatter plot for active dots
active_dots_plot = ax.scatter([], [], color='red', s=40, marker='o')

# List to hold active dots: (x, y, remaining_degrees)
active_dots = []

def init():
    line.set_data([], [])
    active_dots_plot.set_offsets(np.empty((0, 2)))  # Initialize with empty 2D array
    return [line, active_dots_plot]

time_log = 0
i = 0

def update(frame):
    global i, time_log, active_dots
    angle_deg = frame
    angle = np.deg2rad(angle_deg)
    
    # Calculate elapsed time (original logic)
    ratio = (rpm / 60) * 1000 / (2 * math.pi)
    seconds = int(ratio * math.radians(angle_deg))
    
    if i >= len(renderer.renderQueue) or seconds == 0:
        i = 0
        time_log = 0

    # Process render queue
    if i < len(renderer.renderQueue):
        if (seconds - time_log) >= renderer.renderQueue[i][0]:
            # Capture current angle for dot positions
            current_angle = angle
            for coord in renderer.renderQueue[i][1]:
                pos = coord
                x = pos * np.cos(current_angle)
                y = pos * np.sin(current_angle)
                active_dots.append((x, y, 180))  # 180 degrees remaining
            time_log += renderer.renderQueue[i][0]
            i += 1

    # Update dot lifetimes
    delta_angle = 3  # Degrees per frame
    active_dots = [(x, y, rem - delta_angle) for (x, y, rem) in active_dots if rem > delta_angle]

    # Update line position
    dx = (line_length / 2) * np.cos(angle)
    dy = (line_length / 2) * np.sin(angle)
    line.set_data([-dx, dx], [-dy, dy])

    # Update active dots
    if active_dots:
        positions = np.array([(x, y) for (x, y, rem) in active_dots])
        active_dots_plot.set_offsets(positions)
    else:
        active_dots_plot.set_offsets(np.empty((0, 2)))  # Reset to empty 2D array

    return [line, active_dots_plot]

# Create animation (120 frames for full rotation)
ani = FuncAnimation(fig, update, frames=np.linspace(0, 360, 120, endpoint=False),
                    init_func=init, blit=True, interval=120 * 60 / rpm)

plt.show()