#define IS_GATEWAY true  // only true if we are flashing the OTA update gateway node (almost never)
#if !(IS_GATEWAY)

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "ota.h"
#include "state.hpp"
#include "fft.h"
#include "beatdetection.h"
#include "bluetooth.h"
#include "servos.hpp"
#include "shaders.hpp"
#include "synchronize.hpp"
#include "trajectory.hpp"


#define BLUETOOTH_DEBUG_MODE 	false
#define PRINT_SUMMARY    		false

#define SERVO_RX_PIN 5
#define SERVO_TX_PIN 6

#define LED_PIN_1         		7  // outside edge of ring, forward side of servo (away from input cable)
#define LED_PIN_2         		44 // outside edge of ring, forward side of servo (toward input cable)
#define LED_PIN_3         		43 // inside edge of ring

int led_count_this_ring = 0; // this is populated in ShaderManager(); can't be done statically because of import ordering
int led_count_this_ring_inside = 0;

const float alpha_short = 1.0 / (50.0 * 1.0); // roughly 1 seconds

int deviceIndex = -1;

State state;

extern const int MAX_LED_PER_RING;

Adafruit_NeoPixel strip1(MAX_LED_PER_RING, LED_PIN_1, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel strip2(MAX_LED_PER_RING, LED_PIN_2, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel strip3(MAX_LED_PER_RING, LED_PIN_3, NEO_GRB + NEO_KHZ800);

ServoManager servoManager;
ServoController servoController;
Synchronizer synchronizer;
TrajectoryPlanner trajectoryPlanner;
ShaderManager shaderManager(strip1, strip2, strip3);

OtaClient ota;

void setup() {
	#if BLUETOOTH_DEBUG_MODE
	Serial.begin(115200);
	setupBluetooth();
	#else

	Serial.begin(115200); // This is already done in synchronizer init

	delay(5000);
	Serial.println("Booting OTA");

	ota.begin();

	Serial.println("Waiting for 10 seconds...");
	delay(10000);


	Serial1.begin(115200, SERIAL_8N1, SERVO_RX_PIN, SERVO_TX_PIN);
	Serial.println("Waiting for serial to connect...");
	delay(1000);
	Serial.println("Done waiting");

	Synchronizer::instance = &synchronizer;
	Serial.println("Synchronizer instance created");
	synchronizer.init();

	// This must be done after synchronizer.init()
	shaderManager.init();
	shaderManager.setupLedStrips(state.brightness);

	if (synchronizer.role == MASTER) {
		setupAsyncSampling();
		setupBluetooth();
	} else if (synchronizer.role == RING) {
		servoController.setupServo();
		// setupBluetooth(); // TODO: this is for debug and should normally only run on master controller
	} else if (synchronizer.role == BASE) {
		// Do nothing
	} else {
		Serial.print("Unknown role: ");
		Serial.println(synchronizer.role);
		delay(100000);
	}
	
	
	#endif // BLUETOOTH_DEBUG_MODE
}

void loop() {
	Serial.println("asdf");

	#if BLUETOOTH_DEBUG_MODE
	delay(500);
	Serial.println(receivedValue);
	Serial.print("Active shader: ");
	Serial.println(shaderManager.activeShader->getName());
	Serial.print("Servo speeds: ");
	Serial.println(servoManager.getServoSpeeds());

	#else

    // On the master node, step the trajectory forward at 10 RPM
	if (synchronizer.role == MASTER) {
		trajectoryPlanner.update(state);
		state.frame++;
		state.time = millis();
	}

	if (state.frame % 30 == 0) {
		uint8_t brightness = state.brightness;
		if (deviceIndex == 5)	{
			brightness = 255; // sphere always at max brightness
		}
		shaderManager.setBrightness(brightness);
	}

	if (synchronizer.role == MASTER) {
		state.beat_intensity = computeBeatHeuristic();
	} else if (synchronizer.role == RING) {
		servoController.runServo();
		shaderManager.run(state.frame, state.beat_intensity);
	} else if (synchronizer.role == BASE) {
		shaderManager.run(state.frame, state.beat_intensity);	
	}

	#if PRINT_SUMMARY
		Serial.print("Updates per second: ");
		Serial.print(state.updatesPerSecond);
		Serial.print("  |  Beat heuristic: ");
		Serial.println(state.beat_intensity);
		// Serial.print("Active shader: ");
		// Serial.println(shaderManager.activeShader->getName());
		// Serial.print("Active accent shader: ");
		// Serial.println(shaderManager.activeAccentShader->getName());
		// Serial.print("Servo speeds");
		// Serial.println(servoManager.getServoSpeeds());
	#endif // PRINT_SUMMARY

	// Send synchronization data via ESP-NOW
	synchronizer.synchronize();

	#endif // BLUETOOTH_DEBUG_MODE

	float updatesPerSecondImmediate = 1000.0 / float(millis() - state.lastUpdate);
	state.lastUpdate = millis();
	state.updatesPerSecond = alpha_short * updatesPerSecondImmediate + (1.0 - alpha_short) * state.updatesPerSecond;
}


#else // IS_GATEWAY
#include "gateway.hpp"
void setup() { setupGateway(); }
void loop() { loopGateway(); }
#endif // !(IS_GATEWAY)
