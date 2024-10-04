import numpy as np
from vpython import *
from scipy.spatial.transform import Rotation as R

USE_XYXXYX = False
USE_SOLVER = False
USE_INFINITESIMAL = False

MAX_RPM = 60

# Set up the scene
scene = canvas(title='Nested Rotating Rings Simulation',
               width=900, height=800, background=color.black,
               resizeable=False)
scene.forward = vector(-1, -1, -1)
scene.caption = '\n'  # Add space for sliders and controls

# Parameters
num_rings = 6  # Total number of rings

radii = np.array([28, 24, 20, 16, 12, 8, 32])  # Radii of the rings in inches
ball_radius = 8 * 0.0254 * .5
radii = [r * 0.0254 * .5 for r in radii]  # Convert inches to meters

# Initialize angular velocities in RPM
omega_rpm = [30] * num_rings  # initial values in RPM

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
    if i == num_rings - 1:
        # Add a sphere at the center for the innermost ring
        sphere(
            pos=vector(0, 0, 0),
            radius=ball_radius,
            color=color.white,
            shininess=1.0,
            opacity=0.2,
            emissive=False
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
                     shaftwidth=1e-9,
                     headwidth=0.02,
                     headlength=0.06,
                     color=rings[i].color)
    up_arrows.append(up_arrow)
    down_arrow = arrow(pos=vector(0, 0, 0),
                       axis=rings[i].up * -1 * arrow_length,
                       shaftwidth=1e-9,
                       headwidth=0.02,
                       headlength=0.06,
                       color=rings[i].color)
    down_arrows.append(down_arrow)

# Initialize time scale for adjusting simulation speed
time_scale = 1.0  # Default time scale

# Function to update angular velocities when sliders change
def update_omega(index, value):
    omega_rpm[index] = value
    angular_velocities[index] = value * 2 * np.pi / 60  # Convert RPM to rad/s

    rpm_labels[index].text = f'{omega_rpm[index]:.2f} RPM'

# Create sliders and associated text labels
sliders = []
rpm_labels = []

spin = True

def toggle_solver(evt):
    global USE_SOLVER
    if evt.checked:
        USE_SOLVER=True
    else:
        USE_SOLVER=False

rcheck = checkbox(bind=toggle_solver, text='Fixed center ring', checked=USE_SOLVER)
scene.append_to_caption('\n\n')

# Create time scale slider
scene.append_to_caption('Time Scale: ')
def update_time_scale(s):
    global time_scale
    time_scale = s.value

time_slider = slider(bind=update_time_scale, min=0.1, max=2, value=0.5, length=200)
scene.append_to_caption('\n\n')

for i in range(num_rings):
    scene.append_to_caption(f'Omega {i+1} (RPM): ')

    def update(evt, i=i):
        update_omega(i, evt.value)

    s = slider(bind=update, min=-180, max=180, value=omega_rpm[i], length=200)
    sliders.append(s)

    rpm_label = wtext(text=f'{omega_rpm[i]:.2f} RPM')
    rpm_labels.append(rpm_label)

    scene.append_to_caption('\n')  # New line after each slider

# Simulation parameters
dt = 0.005  # Time step in seconds

# Animation loop
t = 0
while True:
    rate(100)  # Limit the simulation to 100 iterations per second
    t += dt * time_scale  # Adjust time increment by time scale

    # Store previous angles for angular velocity calculations
    previous_angles = angles.copy()

    if USE_SOLVER:
        if num_rings == 6:
            dthetas = []
            for i in range(3):
                omega = angular_velocities[i]
                dtheta = omega * dt * time_scale
                dthetas.append(dtheta)
                angles[i] += dtheta  # Update the relative angle

            z_rot_speed = 0 * 2 * np.pi / 60
            z_rot = R.from_rotvec([0, 0, z_rot_speed * t])

            if USE_INFINITESIMAL:
                inv_d_rot = R.from_euler("xyx", -1 * np.array(dthetas[0:3]))
                inverse_dtheta = inv_d_rot.as_euler("yxy")

                for i in range(3, 6):
                    dtheta = inverse_dtheta[i - 3]
                    new_angle = angles[i] + dtheta
                    angles[i] = new_angle
            else:
                inv_rot = R.from_euler("xyx", -1 * np.array(angles[0:3]))
                inverse_angles = (inv_rot * z_rot).as_euler("yxy")[::-1]

                for i in range(3, 6):
                    # new_angle = inverse_angles[i - 3]
                    dtheta = inverse_angles[i - 3] - angles[i]
                    dtheta_rpm = dtheta * (60 / (2 * np.pi)) / (dt * time_scale)
                    if abs(dtheta_rpm) > MAX_RPM:
                        print(f"Exceeded max RPM: {dtheta_rpm}")
                        dtheta_rpm_clipped = np.clip(dtheta_rpm, -MAX_RPM, MAX_RPM)
                        dtheta_clipped = dtheta_rpm_clipped * ((2 * np.pi) / 60) * (dt * time_scale)
                        new_angle = angles[i] + dtheta_clipped
                    else:
                        new_angle = angles[i] + dtheta
                    
                    angles[i] = new_angle

            # Update angular velocities for rings 4, 5, 6
            for i in range(3, 6):
                delta_angle = angles[i] - previous_angles[i]
                angular_velocities[i] = delta_angle / (dt * time_scale)
                omega_rpm[i] = angular_velocities[i] * 60 / (2 * np.pi)
                sliders[i].value = omega_rpm[i]
                rpm_labels[i].text = f'{omega_rpm[i]:.2f} RPM'

        elif num_rings == 4:
            for i in range(2):
                omega = angular_velocities[i]
                dtheta = omega * dt * time_scale
                angles[i] += dtheta  # Update the relative angle

            inverse_angles = R.from_euler("xyz", [-angles[0], -angles[1], 0]).as_euler("xyz")
            for i in range(2, 4):
                angles[i] = inverse_angles[i - 2]

            # Update angular velocities for rings 3, 4
            for i in range(2, 4):
                delta_angle = angles[i] - previous_angles[i]
                angular_velocities[i] = delta_angle / (dt * time_scale)
                omega_rpm[i] = angular_velocities[i] * 60 / (2 * np.pi)
                sliders[i].value = omega_rpm[i]
                rpm_labels[i].text = f'{omega_rpm[i]:.2f} RPM'

        else:
            raise ValueError("Unsupported number of rings")

    else:
        for i in range(num_rings):
            omega = angular_velocities[i]
            dtheta = omega * dt * time_scale
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
