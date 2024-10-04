import numpy as np
from vpython import *
from scipy.spatial.transform import Rotation as R

USE_XYXXYX = False

# Set up the scene
scene = canvas(title='Nested Rotating Rings Simulation',
               width=900, height=900, background=color.black,
               resizeable=False)
scene.forward = vector(-1, -1, -1)
scene.caption = '\n'  # Add space for sliders and controls

# Parameters
num_rings = 6  # Total number of rings
radii = [28, 24, 20, 16, 12, 8, 32]  # Radii of the rings in inches
radii = [r * 0.0254 * .5 for r in radii]  # Convert inches to meters

# Initialize angular velocities in RPM
omega_rpm = [60, 60, 60, -60, -60, -60]  # initial values in RPM
# omega_rpm = [60 for _ in range(num_rings)]  # initial values in RPM
# omega_rpm = [60, -60, 60, -60]  # initial values in RPM
# omega_rpm = [60, 60, 60, -60, -60*2., -60]

# Convert angular velocities to radians per second
angular_velocities = [omega * 2 * np.pi / 60 for omega in omega_rpm]

colors = [
    color.red,
    color.orange,
    color.yellow,
    color.green,
    color.blue,
    color.purple
]

# Initialize ring objects
rings = []

# Initialize angles for each ring relative to the ring outside of it
angles = [0.0 for _ in range(num_rings)]

# Define initial axis and up vectors for each ring
initial_axes = []
initial_ups = []

def rot_axis_from_index(i):
    if USE_XYXXYX:
        if i in [0, 2, 3, 5]:
            return np.array([1, 0, 0])
        else:
            return np.array([0, 1, 0])
    else:
        if i % 2 == 0:
            return np.array([1, 0, 0])  # Rotate about x-axis
        else:
            return np.array([0, 1, 0])

for i in range(num_rings):
    up = rot_axis_from_index(i)
    rings.append(
        ring(pos=vector(0, 0, 0),
             axis=vector(0, 0, 1),
             radius=radii[i],
             thickness=0.006,
             color=colors[i],
             up=vector(*up)
        )
    )
    # Initial axis (normal to the ring plane)
    initial_axes.append(np.array([0, 0, 1]))
    # Initial up vector (in the plane of the ring)
    initial_ups.append(up)

# Create arrows to visualize the up vectors
up_arrows = []
down_arrows = []
arrow_lengths = []
for i in range(num_rings):
    arrow_length = radii[i] * 1.0  # Length proportional to ring size
    arrow_lengths.append(arrow_length)
    up_arrow = arrow(pos=vector(0, 0, 0),
                     axis=rings[i].up * arrow_length,
                     shaftwidth=0.005,
                     color=rings[i].color)
    up_arrows.append(up_arrow)
    down_arrow = arrow(pos=vector(0, 0, 0),
                     axis=rings[i].up * -1 * arrow_length,
                     shaftwidth=0.005,
                     color=rings[i].color)
    down_arrows.append(down_arrow)

# # Function to update angular velocities when sliders change
# def update_omega(index, value):
#     omega_rpm[index] = value
#     angular_velocities[index] = value * 2 * np.pi / 60  # Convert RPM to rad/s

#     # Update dependent angular velocities if needed
#     if index == 2:  # omega3 changed
#         omega_rpm[3] = -omega_rpm[2]
#         sliders[3].value = omega_rpm[3]
#         angular_velocities[3] = omega_rpm[3] * 2 * np.pi / 60
#     if index == 0 or index == 2:  # omega1 or omega3 changed
#         omega_rpm[4] = - (omega_rpm[0] + omega_rpm[2])
#         sliders[4].value = omega_rpm[4]
#         angular_velocities[4] = omega_rpm[4] * 2 * np.pi / 60
#     if index == 1:  # omega2 changed
#         omega_rpm[5] = -omega_rpm[1]
#         sliders[5].value = omega_rpm[5]
#         angular_velocities[5] = omega_rpm[5] * 2 * np.pi / 60

# # Create sliders and associated text labels
# sliders = []
# for i in range(num_rings):
#     scene.append_to_caption(f'Omega {i+1} (RPM): ')

#     def update(evt):
#         update_omega(i, evt.value)

#     s = slider(bind=update, min=-180, max=180, value=omega_rpm[i], length=200)
#     sliders.append(s)

#     scene.append_to_caption('\n')  # New line after each slider

# Simulation parameters
dt = 0.0005  # Time step in seconds

# Animation loop
t = 0
while True:
    rate(100)  # Limit the simulation to 100 iterations per second
    t += dt

    USE_SOLVER = False

    if USE_SOLVER:
        for i in range(3):
            omega = angular_velocities[i]
            dtheta = omega * dt
            angles[i] += dtheta  # Update the relative angle

        inverse_angles = R.from_euler("xyx", -1 * np.array(angles[0:3])).as_euler("yxy")
        for i in range(3, 6):
            angles[i] = inverse_angles[i-3]
    
    else:
        for i in range(num_rings):
            omega = angular_velocities[i]
            dtheta = omega * dt
            angles[i] += dtheta  # Update the relative angle


    # Compute cumulative rotations and update ring orientations
    for i in range(num_rings):

        cumulative_rotation_matrix = np.eye(3)

        for j in range(i + 1):
            angle = angles[j]
            R_j = R.from_rotvec(angle * rot_axis_from_index(j)).as_matrix()
            cumulative_rotation_matrix = cumulative_rotation_matrix @ R_j  # Left multiply for intrinsic rotations

        rotated_axis = cumulative_rotation_matrix @ initial_axes[i]
        rotated_up = cumulative_rotation_matrix @ initial_ups[i]

        # Update the ring's orientation
        rings[i].axis = vector(*rotated_axis)
        rings[i].up = vector(*rotated_up)

        # Update the up_arrow position and orientation
        # up_arrows[i].pos = vector(0, 0, 0) + vector(*rotated_axis) * radii[i]
        up_arrows[i].axis = vector(*rotated_up) * radii[i-1]
        down_arrows[i].axis = vector(*rotated_up) * -1 * radii[i-1]
