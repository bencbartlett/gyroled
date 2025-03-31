import numpy as np
from vpython import *
from scipy.spatial.transform import Rotation as R

PAUSED = False  # Flag to control simulation state

USE_XYXXYX = False
USE_SOLVER = True
CLIP_RPM = False
PREDICTIVE_AVOIDANCE = True
MAX_RPM = 60

NUM_RINGS = 6  # Total number of rings

# Set up the scene
scene = canvas(title='<h1>Totem v2 Sim - Euler Angle Solver</h1>',
               width=700, height=500, background=color.black,
               resizeable=False)
scene.forward = vector(-1, -1, -1)
scene.caption = '\n'  # Add space for sliders and controls

# radii = np.array([28, 24, 20, 16, 12, 8, 32])  # Radii of the rings in inches
meter_per_inch = 0.0254  # Scale factor to convert inches to meters
radii = np.array([36, 32, 28, 24, 20, 10, 40])  # Radii of the rings in inches
ball_radius = 5 * 0.0254
radii = [r * 0.0254 * .5 for r in radii]  # Convert inches to meters

# Initialize angular velocities in RPM
omega_rpm = [30] * NUM_RINGS  # initial values in RPM
omega_rpm = .5 * np.array([30, 34.1, 39.3, 0, 0, 0])
# omega_rpm = .5 * np.array([0, 30, 0, 0, 0, 0])

# Convert angular velocities to radians per second
base_angular_velocities = np.array([omega * 2 * np.pi / 60 for omega in omega_rpm])
angular_velocity_corrections = np.array([0.0 for _ in range(NUM_RINGS)])

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
angles = np.array([0.0 for _ in range(NUM_RINGS)])
# angles = np.pi * np.array([0, .1, .2, .3, .4, .5])  # Initial angles in radians
dthetas = np.array([0.0 for _ in range(NUM_RINGS)])

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

# Create a visual arrow to depict outer_rotation.as_rotvec()
outer_rot_arrow = arrow(pos=vector(0, 0, 0), axis=vector(0, 0, 0), color=color.cyan, shaftwidth=0.02, headwidth=0.04, headlength=0.06)

# Create a visual arrow to depict outer_rotation.as_matrix() @ [0, 0, 1]
outer_axis_arrow = arrow(pos=vector(0, 0, 0), axis=vector(0, 0, 0), color=color.green, shaftwidth=0.02, headwidth=0.04, headlength=0.06)

# Create interactive plot for angular velocities
graph1 = graph(title='Angular Velocities', xtitle='Time (s)', ytitle='Angular Velocity (RPM)', width=700, height=300, background=color.black,
               ymin=-1.3 * MAX_RPM, ymax=1.3 * MAX_RPM, xtitlecolor=color.white, ytitlecolor=color.white, xaxiscolor=color.white, yaxiscolor=color.white)
gcurves = []
for i in range(NUM_RINGS):
    gcurves.append(gcurve(graph=graph1, color=colors[i]))#, label=f'Ring {i+1}'))

# Create interactive plot for ring angles
graph2 = graph(title='Angles (/ pi)', xtitle='Time (s)', ytitle='Angle (rad)', width=700, height=300, background=color.black,
               ymin = -2.1, ymax = 2.1, xtitlecolor=color.white, ytitlecolor=color.white, xaxiscolor=color.white, yaxiscolor=color.white)
angle_curves = []
for i in range(NUM_RINGS):
    angle_curves.append(gcurve(graph=graph2, color=colors[i]))

# Initial plot to make sure the graph is displayed
for i in range(NUM_RINGS):
    gcurves[i].plot(0, omega_rpm[i])
    angle_curves[i].plot(0, angles[i] / np.pi)

# Initialize time scale for adjusting simulation speed
time_scale = 1.0  # Default time scale

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

# Function to update angular velocities when sliders change
def update_omega(index, value):
    omega_rpm[index] = value
    angular_velocities[index] = value * 2 * np.pi / 60  # Convert RPM to rad/s

    # rpm_labels[index].text = f'{omega_rpm[index]:.2f} RPM'

def toggle_solver(evt):
    global USE_SOLVER
    USE_SOLVER = evt.checked
checkbox(bind=toggle_solver, text='Fixed center ring', checked=USE_SOLVER)

def toggle_clip(evt):
    global CLIP_RPM
    CLIP_RPM = evt.checked
checkbox(bind=toggle_clip, text='Clip RPM', checked=CLIP_RPM)

def toggle_clip(evt):
    global PREDICTIVE_AVOIDANCE
    PREDICTIVE_AVOIDANCE = evt.checked
checkbox(bind=toggle_clip, text='Predictive Avoidance', checked=PREDICTIVE_AVOIDANCE)


scene.append_to_caption('\n\n')

# Create time scale slider
scene.append_to_caption('Time Scale: ')
def update_time_scale(s):
    global time_scale
    time_scale = s.value

time_slider = slider(bind=update_time_scale, min=0.1, max=2, value=0.5, length=200)
scene.append_to_caption('\n\n')

# scene.append_to_caption('Time Control:\n')

# def update_time_slider(s):
#     global t
#     if PAUSED:
#         t = s.value
#     t_label.text = f"Time: {t:.2f} s"

# time_slider_t = slider(bind=update_time_slider, min=0, max=100, value=0, length=200)

# t_label = wtext(text=f"Time: {0:.2f} s")
# scene.append_to_caption('\n\n')

for i in range(NUM_RINGS):
    scene.append_to_caption(f'Omega {i+1} (RPM): ')

    def update(evt, i=i):
        update_omega(i, evt.value)

    s = slider(bind=update, min=-60, max=60, value=omega_rpm[i], length=200)
    sliders.append(s)

    rpm_label = wtext(text=f'{omega_rpm[i]:.2f} RPM')
    rpm_labels.append(rpm_label)

    scene.append_to_caption('\n')  # New line after each slider

dt = 0.005  # Time step in seconds


gimbal_lock_cutoff = 0.3  # Adjustable cutoff value for outer rotation norm



# TODO: rather than computing angles at each step to cancel out the angles applied by the first three rings, let's instead calculate the current orientation of the center ring and then compute the operation needed to bring the center ring toward the desired heading.




# Animation loop
# breakpoint_times = [3.53]

t = 0
while True:
    if PAUSED:
        continue
    rate(100)
    t += dt * time_scale
    # time_slider_t.value = t
    # t_label.text = f"Time: {t:.2f} s"

    # Store previous angles for angular velocity calculations
    previous_angles = angles.copy()

    if USE_SOLVER:
        for i in range(3):
            omega = base_angular_velocities[i] + angular_velocity_corrections[i]
            dthetas[i] = omega * dt * time_scale
            angles[i] += dthetas[i]  # Update the relative angle
            angles[i] = angles[i] % (2 * np.pi)

        if PREDICTIVE_AVOIDANCE:
            # New predictive control to avoid gimbal lock
            avg_outer_omega = np.mean(np.abs(base_angular_velocities[0:3]))
            if avg_outer_omega > 0:
                look_ahead_dt = (np.pi/4) / avg_outer_omega
            else:
                look_ahead_dt = 0

            predicted_outer_angles = angles[0:3] + (base_angular_velocities[0:3] + angular_velocity_corrections[0:3]) * look_ahead_dt
            # outer_rotation_future = R.from_euler("xyx", predicted_outer_angles)
            
            outer_rotation_future = (
                R.from_rotvec(predicted_outer_angles[0] * rot_axis_from_index(0))
                * R.from_rotvec(predicted_outer_angles[1] * rot_axis_from_index(1))
                * R.from_rotvec(predicted_outer_angles[2] * rot_axis_from_index(2))
            )
            outer_rotation_future_magnitude = np.linalg.norm(outer_rotation_future.as_rotvec())

            if outer_rotation_future_magnitude < gimbal_lock_cutoff:
                print(f"FUTURE outer rotation vector: {outer_rotation_future.as_rotvec()}, {outer_rotation_future_magnitude=}")
                    
                angle_correction = R.from_rotvec(
                    outer_rotation_future.as_rotvec() * gimbal_lock_cutoff / outer_rotation_future_magnitude
                ).as_euler("XYX")
                # Compute the angular velocity correction
                angular_velocity_corrections[0:3] = angle_correction / look_ahead_dt
                
                print(f"Gimbal lock avoidance activated")
                print(f"Additional correction: {angle_correction}")
                print(f"Angular velocity correction: {angular_velocity_corrections}")

            else:
                # No gimbal lock avoidance needed
                angular_velocity_corrections[0:3] = 0

        z_rot_speed = 0 * 2 * np.pi / 60
        z_rot = R.from_rotvec([0, 0, z_rot_speed * t])


        outer_rotation = R.from_euler("xyx", np.array(angles[0:3]))
        # outer_rotation = (
        #     R.from_rotvec(angles[0] * rot_axis_from_index(0))
        #     * R.from_rotvec(angles[1] * rot_axis_from_index(1))
        #     * R.from_rotvec(angles[2] * rot_axis_from_index(2))
        # )

        desired_inner_rotation = outer_rotation.inv()
        desired_inner_angles = (desired_inner_rotation * z_rot).as_euler("yxy")

        # If the angle jumps from -180 to 180, this is acutally a small change in the negative 
        # direction rather than a large change in the positive direction. Let's adjust the computed
        # dtheta values to reflect this.


        def unwrap_pi(delta):
            # Unwrap the delta modulo pi to choose the equivalent angle closest to zero
            # return ((delta + np.pi/2) % np.pi) - np.pi/2

            return ((delta + np.pi) % (2 *np.pi)) - np.pi

        for i in range(3, 6):
            raw_dtheta = desired_inner_angles[i - 3] - previous_angles[i]
            dthetas[i] = raw_dtheta
            dthetas[i] = unwrap_pi(raw_dtheta)

            dtheta_rpm = dthetas[i] * (60 / (2 * np.pi)) / (dt * time_scale)

            if abs(dthetas[i]) > .9 * np.pi:
                print(f"Dtheta jump for ring {i+1=}! {dthetas[i]}")

            if CLIP_RPM and abs(dtheta_rpm) > MAX_RPM:
                print(f"Clipping to max RPM! {dtheta_rpm}")
                dtheta_rpm_clipped = np.clip(dtheta_rpm, -MAX_RPM, MAX_RPM)
                dthetas[i] = dtheta_rpm_clipped * ((2 * np.pi) / 60) * (dt * time_scale)

            new_angle = angles[i] + dthetas[i]
            base_angular_velocities[i] = dthetas[i] / (dt * time_scale)

            angles[i] = new_angle

    else:
        for i in range(NUM_RINGS):
            omega = base_angular_velocities[i]
            dtheta = omega * dt * time_scale
            angles[i] += dtheta  # Update the relative angle

    # Compute cumulative rotations and update ring orientations

    cumulative_rotation_matrices = []

    for i in range(NUM_RINGS):

        cumulative_rotation_matrix = np.eye(3)
        for j in range(i + 1):
            angle = angles[j]
            R_j = R.from_rotvec(angle * rot_axis_from_index(j)).as_matrix()
            cumulative_rotation_matrix = cumulative_rotation_matrix @ R_j  # Left multiply for intrinsic rotations
        
        cumulative_rotation_matrices.append(cumulative_rotation_matrix)

        rotated_axis = cumulative_rotation_matrix @ initial_axes[i]
        rotated_up = cumulative_rotation_matrix @ initial_ups[i]

        # Update the ring's orientation
        rings[i].axis = vector(*rotated_axis)
        rings[i].up = vector(*rotated_up)

        # Update the up_arrow position and orientation
        # up_arrows[i].pos = vector(0, 0, 0) + vector(*rotated_axis) * radii[i]
        up_arrows[i].axis = vector(*rotated_up) * radii[i-1]
        down_arrows[i].axis = vector(*rotated_up) * -1 * radii[i-1]

        omega_rpm[i] = (base_angular_velocities[i] + angular_velocity_corrections[i]) * 60 / (2 * np.pi)
        sliders[i].value = omega_rpm[i]
        rpm_labels[i].text = f'{omega_rpm[i]:.2f} RPM'

    outer_rotation = (
        R.from_rotvec(angles[0] * rot_axis_from_index(0))
        * R.from_rotvec(angles[1] * rot_axis_from_index(1))
        * R.from_rotvec(angles[2] * rot_axis_from_index(2))
    )
    # outer_rotation = R.from_matrix(cumulative_rotation_matrices[2])
    outer_rotation_vec = outer_rotation.as_rotvec()
    outer_rotation_vec_magnitude = np.linalg.norm(outer_rotation_vec)
    print(f"Outer rotation vector: {outer_rotation_vec}, mag={outer_rotation_vec_magnitude}")
    if outer_rotation_vec_magnitude < gimbal_lock_cutoff:
        outer_rot_arrow.axis = vector(*(10 * outer_rotation_vec * meter_per_inch * 36/2 / np.pi)) 
        outer_rot_arrow.color = color.red
    else:
        outer_rot_arrow.axis = vector(*(outer_rotation_vec * meter_per_inch * 36/2 / np.pi)) 
        outer_rot_arrow.color = color.cyan

    # Compute the rotated Z-axis of the outer rotation
    rotated_y = outer_rotation.as_matrix() @ np.array([0, 1, 0])
    outer_axis_arrow.axis = vector(*(rotated_y * radii[3]))

    
    if t > .1 and any(abs(omega) > MAX_RPM for omega in omega_rpm):
        breakpoint()

    def colorize(value):
        if abs(value) > MAX_RPM:
            return f"\033[91m{value:.2f}\033[0m"  # Red color
        return f"{value:.2f}"
    
    # print([float(angle) for angle in angles])
    print(f"θ1={colorize(angles[0])} θ2={colorize(angles[1])} θ3={colorize(angles[2])} θ4={colorize(angles[3])} θ5={colorize(angles[4])} θ6={colorize(angles[5])}  |  ω1={colorize(omega_rpm[0])} ω2={colorize(omega_rpm[1])} ω3={colorize(omega_rpm[2])} ω4={colorize(omega_rpm[3])} ω5={colorize(omega_rpm[4])} ω6={colorize(omega_rpm[5])}")
    
    # Update interactive plot with current angular velocities
    for i in range(NUM_RINGS):
        gcurves[i].plot(t, omega_rpm[i])

    # Update interactive plot with current ring angles
    for i in range(NUM_RINGS):
        angle_curves[i].plot(t, angles[i] / np.pi)
