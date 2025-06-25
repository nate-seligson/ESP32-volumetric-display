import matplotlib.pyplot as plt
import render

r = render.Renderer(rpm = 20)

# Example list of (x, y) positions
dot_positions = r.testQueue

# Unzip the list of tuples into two lists: x_coords and y_coords
x_coords, y_coords = zip(*dot_positions)

# Create the plot
plt.figure(figsize=(8, 8))
plt.scatter(x_coords, y_coords, color='blue', label='Dots')

# Set the axis limits to -64 to 64
plt.xlim(-64, 64)
plt.ylim(-64, 64)

# Add grid, labels, and title
plt.grid(True)
plt.xlabel('X Coordinate')
plt.ylabel('Y Coordinate')
plt.title('Plot of Dots (Clamped from -64 to 64)')

# Add a legend
plt.legend()

# Show the plot
plt.show()