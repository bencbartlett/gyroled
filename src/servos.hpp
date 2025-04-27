#include <Arduino.h>
#include <LSS.h>
#include <cmath>

#include "state.hpp"


// ID set to default LSS ID = 0
#define LSS_ID		(0)  // This is 0 for all servos since there is only one servo per ring
#define LSS_BAUD	(LSS_DefaultBaud)
// Choose the proper serial port for your platform
// #define LSS_SERIAL	(Serial)	
#define LSS_SERIAL  (Serial1)  // Uses tx and rx UART pins

// #define SERVO_1_PIN     25
// #define SERVO_2_PIN     26
// #define SERVO_3_PIN     27
// #define SERVO_UPDATE_HZ 50

#define SERVO_DEBUG_MODE false

// extern float updatesPerSecond; // from main.cpp
extern State state;

/**
 * Run by controllers on each ring to locally drive that ring's servo.
 */
class ServoController {
private:
	LSS servo = LSS(LSS_ID);
	float error = 0.0f;
	float prev_error = 0.0f;

	float curr_time = 0.0f;
	float prev_time = -1.0f; // -1.0f to avoid division by zero on first run

	/* ------------------ PD speed controller ------------------ */
	// Proportional‑Derivative gains (tune as needed)
	float Kp = 1.2f;   // tuned for 30 kg·cm servo & ring inertia
	float Kd = 0.20f;  

	float wrap360(float a) {
		a = fmodf(a, 360.0f);
		return (a < 0) ? a + 360.0f : a;
	};

public:
	int   ringIndex        = 0;          // 1…6  (0 = master / unused)
	unsigned long lastStateReceived = 0; // ms
	float target_angle = 0.0;  // Target angle in degrees for this servo
	float target_angular_velocity = 0.0; // Target angular velocity in degrees per second
	float current_angle = 0.0;
	// float current_rpm = 0.0;
	// float target_rpm = 0.0;

	void setupServo() {
		// Initialize the LSS bus
		LSS::initBus(LSS_SERIAL, LSS_BAUD);
		// Wait for the LSS to boot
		delay(1000);

		servo.setAngularStiffness(0);
		// servo.setAngularHoldingStiffness(0);
		// servo.setAngularAcceleration(15);
		// servo.setAngularDeceleration(15);

	}

	void runServo() {
	    if (state.isPaused) {
	        servo.hold();
	    } else {
			#if USE_CUSTOM_PD_CONTROLLER
			/* --- predict where the master wants me *now* --- */
			float dt = (millis() - lastStateReceived) / 1000.0f;   // seconds
			float predicted = wrap360(target_angle + target_angular_velocity * dt); // linear extrapolation
			if (predicted < 0) predicted += 360.0f;

			float error = wrap360(predicted) - wrap360(current_angle);		
			if (error >  180.0f) error -= 360.0f;
			if (error < -180.0f) error += 360.0f;
	
	        /* ----- PD control with derivative of the wrapped error ----- */
			curr_time = millis() / 1000.0f;
			float dErr     = (error - prev_error) / (curr_time - prev_time);            // deg/s
			prev_time = curr_time;
			prev_error = error; 
	
	        // --- PD control law using error and its derivative ---
	        float command_deg_per_s = Kp * error + Kd * dErr;
	
	        // --- Convert to 1/10°/s for wheel() ---
	        int16_t speedCmd = static_cast<int16_t>(command_deg_per_s * 10.0f);
	
	        // Clamp to ±10 RPM  (≈ ±60 deg/s  →  ±600 × 1/10°/s)
	        // const int16_t MAX_CMD = 600;
	        // if (speedCmd >  MAX_CMD) speedCmd =  MAX_CMD;
	        // if (speedCmd < -MAX_CMD) speedCmd = -MAX_CMD;
	
	        bool result = servo.wheel(speedCmd);
			if (!result) {
				Serial.println("Error sending command to servo.");
			}

			Serial.printf("Err: %6.1f°  dErr: %6.1f°/s  Cmd: %4d (1/10°/s)\n",
						error, dErr, speedCmd);
			#else

			// // Use direct move commands to the servo and use the built in virtual position system
			// // instead of wrapping around 360 degrees
			// float dt = (millis() - lastStateReceived) / 1000.0f;   // seconds
			// float predicted = target_angle + target_angular_velocity * dt; // linear extrapolation
			
			// float prev_angle = current_angle;

			// servo.moveRelative(static_cast<int16_t>(predicted * 10.0f)); // Move to the predicted position in 1/10° units
			
			// Run at 10rpm 
			float RPM = 8.0f;
			float DEGS_PER_SEC = RPM * 360.0f / 60.0f;
			servo.wheel(10 * DEGS_PER_SEC);

			#endif

	    }
		current_angle = wrap360((servo.getPosition()) / 10.0f);

		// Pretty print a table
		Serial.println(F("\nRing   Target°   Currnt°   ω (°/s)"));
		Serial.println(F("----   --------  --------   --------"));
		Serial.printf("  1    %8.1f   %8.1f   %8.1f\n", state.target_angle_1, ringIndex == 1 ? current_angle : 0., state.target_angular_velocity_1);
		Serial.printf("  2    %8.1f   %8.1f   %8.1f\n", state.target_angle_2, ringIndex == 2 ? current_angle : 0., state.target_angular_velocity_2);
		Serial.printf("  3    %8.1f   %8.1f   %8.1f\n", state.target_angle_3, ringIndex == 3 ? current_angle : 0., state.target_angular_velocity_3);
		Serial.printf("  4    %8.1f   %8.1f   %8.1f\n", state.target_angle_4, ringIndex == 4 ? current_angle : 0., state.target_angular_velocity_4);
		Serial.printf("  5    %8.1f   %8.1f   %8.1f\n", state.target_angle_5, ringIndex == 5 ? current_angle : 0., state.target_angular_velocity_5);
		Serial.printf("  6    %8.1f   %8.1f   %8.1f\n", state.target_angle_6, ringIndex == 6 ? current_angle : 0., state.target_angular_velocity_6);	
		Serial.println(F("===========================\n"));
	}

};


/**
 * Run by the master controller to remotely control the servos on each ring.
 */
class ServoManager {
private:
	// Servo servo1;
	// Servo servo2;
	// Servo servo3;
	float servo_master_speed = 0.5; // Can range from 0 to 1
	float servo1_speed = 0.7; // Can range from -1 to 1
	float servo2_speed = 0.85;
	float servo3_speed = 1.0;
	bool updateServo = true; // Flag to control servo update
	unsigned long lastServoUpdate = 0;
public:
	bool hasPhoneEverConnected = false;

	ServoManager() {
		#if SERVO_DEBUG_MODE
		servo_master_speed = 0.0;
		#endif
	}

	void setupServos() {
		// ESP32PWM::allocateTimer(0);
		// ESP32PWM::allocateTimer(1);
		// ESP32PWM::allocateTimer(2);
		// ESP32PWM::allocateTimer(3);

		// servo1.setPeriodHertz(SERVO_UPDATE_HZ);
		// servo2.setPeriodHertz(SERVO_UPDATE_HZ);
		// servo3.setPeriodHertz(SERVO_UPDATE_HZ);
		// servo1.attach(SERVO_1_PIN, 500, 2400);
		// servo2.attach(SERVO_2_PIN, 500, 2400);
		// servo3.attach(SERVO_3_PIN, 500, 2400);
	}

	String getServoSpeeds() {
		return String(servo_master_speed) + ";" + String(servo1_speed) + ";" + String(servo2_speed) + ";" + String(servo3_speed);
	}

	void setServoSpeed(int servo, float speed) {
		switch (servo) {
		case 0:
			servo_master_speed = speed;
			break;
		case 1:
			servo1_speed = speed;
			break;
		case 2:
			servo2_speed = speed;
			break;
		case 3:
			servo3_speed = speed;
			break;
		default:
			Serial.println("Invalid servo number");
			break;
		}
	}

	void runServos() {
		// if (millis() < 1000) {
		// 	// Turn off servos for the first 2s
		// 	updateServo = false;
		// }
		// else if (millis() - lastServoUpdate > 1000 / SERVO_UPDATE_HZ) {
		// 	// Simple non-blocking delay for servo update
		// 	lastServoUpdate = millis();
		// 	updateServo = true;
		// }

		// if (servo_master_speed < 0.1) {
		// 	// Turn servos on after 30s in case I forget to turn off servo debug mode
		// 	if (!hasPhoneEverConnected && millis() > 30000) {
		// 		servo_master_speed = 0.3;
		// 	}
		// }

		// if (updateServo) {
		// 	// Example servo movements
		// 	servo1.write(int(90 + 90 * (servo_master_speed * servo1_speed)));
		// 	servo2.write(int(90 + 90 * (servo_master_speed * servo2_speed)));
		// 	servo3.write(int(90 + 90 * (servo_master_speed * servo3_speed)));
		// 	updateServo = false; // Prevent updating the servo in the next loop iteration
		// }

	}

};
