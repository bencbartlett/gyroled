#include <Arduino.h>
#include <LSS.h>

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

/**
 * Run by slave controllers on each local ring to drive that ring's servo.
 */
class ServoController {
private:
	LSS servo = LSS(LSS_ID);

public:
	void setupServo() {
		// Initialize the LSS bus
		LSS::initBus(LSS_SERIAL, LSS_BAUD);
		// Wait for the LSS to boot
		delay(1000);
	}

	void runServo() {
		// Placeholder for now
		int32_t pos = servo.getPosition();
		uint8_t rpm = servo.getSpeedRPM();
		uint16_t current = servo.getCurrent();
		uint16_t voltage = servo.getVoltage();
		uint16_t temp = servo.getTemperature();

		int32_t targetPos = pos + 100; // 100 * 1/10deg per frame;
		servo.wheel(2000);

		Serial.printf("Pos: %d, RPM: %d, Current: %d, Voltage: %d, Temp: %d\n", pos, rpm, current, voltage, temp);
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
