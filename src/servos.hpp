#include <Arduino.h>
#include <ESP32Servo.h>

#define SERVO_1_PIN     14
#define SERVO_2_PIN     27
#define SERVO_3_PIN     26
#define SERVO_UPDATE_HZ 50

#define SERVO_DEBUG_MODE false

class ServoManager {
private:
	Servo servo1;
	Servo servo2;
	Servo servo3;
	float servo_master_speed = 0.3; // Can range from 0 to 1
	float servo1_speed = 0.6; // Can range from -1 to 1
	float servo2_speed = 0.8;
	float servo3_speed = 1.0;
	bool updateServo = true; // Flag to control servo update
	unsigned long lastServoUpdate = 0;
public:
	ServoManager() {
		#if SERVO_DEBUG_MODE
		servo_master_speed = 0.0;
		#endif
	}

	void setupServos() {
		ESP32PWM::allocateTimer(0);
		ESP32PWM::allocateTimer(1);
		ESP32PWM::allocateTimer(2);
		ESP32PWM::allocateTimer(3);

		servo1.setPeriodHertz(SERVO_UPDATE_HZ);
		servo2.setPeriodHertz(SERVO_UPDATE_HZ);
		servo3.setPeriodHertz(SERVO_UPDATE_HZ);
		servo1.attach(SERVO_1_PIN, 500, 2400);
		servo2.attach(SERVO_2_PIN, 500, 2400);
		servo3.attach(SERVO_3_PIN, 500, 2400);
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
		if (updateServo) {
			// Example servo movements
			servo1.write(int(90 + 90 * (servo_master_speed * servo1_speed)));
			servo2.write(int(90 + 90 * (servo_master_speed * servo2_speed)));
			servo3.write(int(90 + 90 * (servo_master_speed * servo3_speed)));
			updateServo = false; // Prevent updating the servo in the next loop iteration
		}

		// Simple non-blocking delay for servo update
		if (millis() - lastServoUpdate > 1000 / SERVO_UPDATE_HZ) {
			lastServoUpdate = millis();
			updateServo = true;
		}
	}

};
