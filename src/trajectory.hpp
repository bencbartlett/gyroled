#include <Arduino.h>
#include <cmath>
#include "state.hpp"

float RPM = 15.0f;
float DEGS_PER_SEC = RPM * 360.0f / 60.0f;

/**
 * Plans simple constant‑speed trajectories for each ring.
 * Updates the target angles at a fixed RPM.
 */
class TrajectoryPlanner {
public:

    /**
     * Increment each target angle by DEGS_PER_SEC * dt, wrapping at 360°.
     * @param state  The shared State struct to modify.
     * @param dt     Time elapsed since last update, in seconds.
     */
    void update(State& state) {
		float dt = 1. / state.updatesPerSecond;
        state.target_angle_1 = fmodf(state.target_angle_1 + DEGS_PER_SEC * dt*0., 360.0f);
        state.target_angle_2 = fmodf(state.target_angle_2 + DEGS_PER_SEC * dt*0., 360.0f);
        state.target_angle_3 = fmodf(state.target_angle_3 + DEGS_PER_SEC * dt, 360.0f);
        state.target_angle_4 = fmodf(state.target_angle_4 + DEGS_PER_SEC * dt*0., 360.0f);
        state.target_angle_5 = fmodf(state.target_angle_5 + DEGS_PER_SEC * dt*0., 360.0f);
        state.target_angle_6 = fmodf(state.target_angle_6 + DEGS_PER_SEC * dt, 360.0f);
		state.target_angular_velocity_1 = DEGS_PER_SEC;
		state.target_angular_velocity_2 = DEGS_PER_SEC;
		state.target_angular_velocity_3 = DEGS_PER_SEC;
		state.target_angular_velocity_4 = DEGS_PER_SEC;
		state.target_angular_velocity_5 = DEGS_PER_SEC;
		state.target_angular_velocity_6 = DEGS_PER_SEC;
    }

};