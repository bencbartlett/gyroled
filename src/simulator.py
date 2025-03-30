import numpy as np
from vpython import *
from scipy.spatial.transform import Rotation as R

PAUSED = True  # Flag to control simulation state

USE_XYXXYX = False
USE_SOLVER = True
CLIP_RPM = False
MAX_RPM = 60

NUM_RINGS = 6  # Total number of rings

# Set up the scene
scene = canvas(title='<h1>Totem v2 Sim - Euler Angle Solver</h1>',
               width=700, height=700, background=color.black,
               resizeable=False)
scene.forward = vector(-1, -1, -1)
scene.caption = '\n'  # Add space for sliders and controls

# radii = np.array([28, 24, 20, 16, 12, 8, 32])  # Radii of the rings in inches
radii = np.array([36, 32, 28, 24, 20, 10, 40])  # Radii of the rings in inches
ball_radius = 5 * 0.0254
radii = [r * 0.0254 * .5 for r in radii]  # Convert inches to meters

# Initialize angular velocities in RPM
omega_rpm = [30] * NUM_RINGS  # initial values in RPM
omega_rpm = .5 * np.array([30, 35, 40, 0, 0, 0])

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
angles = [0.0 for _ in range(NUM_RINGS)]
dthetas = [0.0 for _ in range(NUM_RINGS)]

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

for i in range(NUM_RINGS):
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
    if i == NUM_RINGS - 1:
        # Add a sphere at the center for the innermost ring
        sphere(
            pos=vector(0, 0, 0),
            radius=ball_radius,
            color=color.white,
            shininess=1.0,
            opacity=0.5,
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
for i in range(NUM_RINGS):
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

# Create pause button
def toggle_simulation(b):
    global PAUSED
    PAUSED = not PAUSED
    if PAUSED:
        b.text = "Pause"
    else:
        b.text = "Play"

pause_button = button(text="Pause", bind=toggle_simulation)
scene.append_to_caption('\n\n')

# Create sliders and associated text labels
sliders = []
rpm_labels = []

def toggle_solver(evt):
    global USE_SOLVER
    USE_SOLVER = evt.checked
checkbox(bind=toggle_solver, text='Fixed center ring', checked=USE_SOLVER)

def toggle_clip(evt):
    global CLIP_RPM
    CLIP_RPM = evt.checked
checkbox(bind=toggle_clip, text='Clip RPM', checked=CLIP_RPM)


scene.append_to_caption('\n\n')

# Create time scale slider
scene.append_to_caption('Time Scale: ')
def update_time_scale(s):
    global time_scale
    time_scale = s.value

time_slider = slider(bind=update_time_scale, min=0.1, max=2, value=0.5, length=200)
scene.append_to_caption('\n\n')

for i in range(NUM_RINGS):
    scene.append_to_caption(f'Omega {i+1} (RPM): ')

    def update(evt, i=i):
        update_omega(i, evt.value)

    s = slider(bind=update, min=-60, max=60, value=omega_rpm[i], length=200)
    sliders.append(s)

    rpm_label = wtext(text=f'{omega_rpm[i]:.2f} RPM')
    rpm_labels.append(rpm_label)

    scene.append_to_caption('\n')  # New line after each slider

# Simulation parameters
dt = 0.005  # Time step in seconds




# TODO: rather than computing angles at each step to cancel out the angles applied by the first three rings, let's instead calculate the current orientation of the center ring and then compute the operation needed to bring the center ring toward the desired heading.




# Animation loop
t = 0
while True:
    if PAUSED:
        continue
    rate(100)  # Limit the simulation to 100 iterations per second
    t += dt * time_scale  # Adjust time increment by time scale

    # Store previous angles for angular velocity calculations
    previous_angles = angles.copy()
    previous_dthetas = dthetas.copy()

    if USE_SOLVER:
        if NUM_RINGS == 6:
            for i in range(3):
                omega = angular_velocities[i]
                dthetas[i] = omega * dt * time_scale
                angles[i] += dthetas[i]  # Update the relative angle
                angles[i] = angles[i] % (2 * np.pi)

            z_rot_speed = 0 * 2 * np.pi / 60
            z_rot = R.from_rotvec([0, 0, z_rot_speed * t])

            inv_rot = R.from_euler("xyx", np.array(angles[0:3])).inv()
            desired_inner_angles = (inv_rot * z_rot).as_euler("yxy")

            # If the angle jumps from -180 to 180, this is acutally a small change in the negative 
            # direction rather than a large change in the positive direction. Let's adjust the computed
            # dtheta values to reflect this.

            for i in range(3, 6):
                dthetas[i] = desired_inner_angles[i - 3] - previous_angles[i]
                dthetas[i] = (dthetas[i] + np.pi) % (2 * np.pi) - np.pi 
                dtheta_rpm = dthetas[i] * (60 / (2 * np.pi)) / (dt * time_scale)

                if CLIP_RPM and abs(dtheta_rpm) > MAX_RPM:
                    print(f"Clipping to max RPM! {dtheta_rpm}")
                    dtheta_rpm_clipped = np.clip(dtheta_rpm, -MAX_RPM, MAX_RPM)
                    dthetas[i] = dtheta_rpm_clipped * ((2 * np.pi) / 60) * (dt * time_scale)

                new_angle = angles[i] + dthetas[i]
                angular_velocities[i] = dthetas[i] / (dt * time_scale)
                omega_rpm[i] = angular_velocities[i] * 60 / (2 * np.pi)
                sliders[i].value = omega_rpm[i]
                rpm_labels[i].text = f'{omega_rpm[i]:.2f} RPM'

                angles[i] = new_angle


        else:
            raise ValueError("Unsupported number of rings")

    else:
        for i in range(NUM_RINGS):
            omega = angular_velocities[i]
            dtheta = omega * dt * time_scale
            angles[i] += dtheta  # Update the relative angle

    # Compute cumulative rotations and update ring orientations
    for i in range(NUM_RINGS):

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

    def colorize(value):
        if abs(value) > MAX_RPM:
            return f"\033[91m{value:.2f}\033[0m"  # Red color
        return f"{value:.2f}"
    
    # print([float(angle) for angle in angles])
    print(f"θ1 {colorize(angles[0])} θ2 {colorize(angles[1])} θ3 {colorize(angles[2])} θ4 {colorize(angles[3])} θ5 {colorize(angles[4])} θ6 {colorize(angles[5])}  |  ω1 {colorize(omega_rpm[0])} ω2 {colorize(omega_rpm[1])} ω3 {colorize(omega_rpm[2])} ω4 {colorize(omega_rpm[3])} ω5 {colorize(omega_rpm[4])} ω6 {colorize(omega_rpm[5])}")
    # print(f"θ1 {angles[0]:.2f} θ2 {angles[1]:.2f} θ3 {angles[2]:.2f} θ4 {angles[3]:.2f} θ5 {angles[4]:.2f} θ6 {angles[5]:.2f}  |  ω1 {omega_rpm[0]:.2f} ω2 {omega_rpm[1]:.2f} ω3 {omega_rpm[2]:.2f} ω4 {omega_rpm[3]:.2f} ω5 {omega_rpm[4]:.2f} ω6 {omega_rpm[5]:.2f}")
