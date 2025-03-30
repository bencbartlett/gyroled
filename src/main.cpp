#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

#include "fft.h"
#include "beatdetection.h"
#include "bluetooth.h"
#include "servos.hpp"
#include "shaders.hpp"
#include "synchronize.hpp"


#define BLUETOOTH_DEBUG_MODE 	false
#define PRINT_SUMMARY    		false
#define OUTPUT_TO_VISUALIZER 	false

#define SERVO_RX_PIN 5 //4
#define SERVO_TX_PIN  6//5

#define LED_PIN_1         		44 //7  // outside edge of ring, forward side of servo (away from input cable)
#define LED_PIN_2         		7 //8  // outside edge of ring, forward side of servo (toward input cable)
#define LED_PIN_3         		43 //6 // inside edge of ring

#define LED_COUNT       	 	130

int frame = 0;
float updatesPerSecond = 30.0;

const float alpha_short = 1.0 / (30.0 * 5.0); // roughly 5 seconds
unsigned long lastUpdate = 0;

int brightness = 255;
Adafruit_NeoPixel strip1(LED_COUNT, LED_PIN_1, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel strip2(LED_COUNT, LED_PIN_2, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel strip3(LED_COUNT, LED_PIN_3, NEO_GRB + NEO_KHZ800);


LedColor ledColors[LED_COUNT];

ShaderManager shaderManager(strip1, strip2, strip3, ledColors);

ServoManager servoManager;
ServoController servoController;
Synchronizer synchronizer;

// float spectrogram[NUM_BANDS] = { };
// float frequencies[SAMPLES / 2] = { };


void setup() {
	#if BLUETOOTH_DEBUG_MODE
	Serial.begin(115200);
	setupBluetooth();
	#else

	Serial.begin(115200);
	Serial1.begin(115200, SERIAL_8N1, SERVO_RX_PIN, SERVO_TX_PIN);

	Serial.println("Waiting for serial to connect...");
	delay(5000);
	Serial.println("Done waiting");


	// servoManager.setupServos();
	shaderManager.setupLedStrips(brightness);
	servoController.setupServo();

	// Initialize the Synchronizer
	Synchronizer::instance = &synchronizer;
	Serial.println("Synchronizer instance created");
	synchronizer.init();

	// Use runtime check instead of compile-time flag
	if (synchronizer.role == MASTER) {
		setupAsyncSampling();
	}
	
	setupBluetooth();
	#endif // BLUETOOTH_DEBUG_MODE
}

void loop() {

	#if BLUETOOTH_DEBUG_MODE
	delay(500);
	Serial.println(receivedValue);
	Serial.print("Active shader: ");
	Serial.println(shaderManager.activeShader->getName());
	Serial.print("Servo speeds: ");
	Serial.println(servoManager.getServoSpeeds());

	#else
	frame++;

	if (frame % 30 == 0) {
		shaderManager.setBrightness(brightness);
	}


	float beatHeuristic = 0.0;
	if (synchronizer.role == MASTER) {
		// computeSpectrogram(spectrogram);
		// doFFT(frequencies);
		beatHeuristic = computeBeatHeuristic();
	}


	// servoManager.runServos();
	servoController.runServo();

	shaderManager.run(frame, beatHeuristic);

	float updatesPerSecondImmediate = 1000.0 / float(millis() - lastUpdate);
	lastUpdate = millis();

	updatesPerSecond = alpha_short * updatesPerSecondImmediate + (1.0 - alpha_short) * updatesPerSecond;

	#if OUTPUT_TO_VISUALIZER
		String foo = "[SPECTROGRAM]:";
		for (uint16_t i = 0; i < NUM_BANDS; i++) {
			// Serial.println(bands[i]); // Send each frequency bin's magnitude
			foo += spectrogram[i];
			if (i < NUM_BANDS - 1) {
				foo += ",";
			}
		}
		foo += ";";
		foo += beatHeuristic;
		Serial.println(foo);
	#endif // OUTPUT_TO_VISUALIZER

	#if PRINT_SUMMARY
		Serial.print("Updates per second: ");
		Serial.print(updatesPerSecond);
		Serial.print("  |  Beat heuristic: ");
		Serial.println(beatHeuristic);
		// Serial.print("Active shader: ");
		// Serial.println(shaderManager.activeShader->getName());
		// Serial.print("Active accent shader: ");
		// Serial.println(shaderManager.activeAccentShader->getName());
		// Serial.print("Servo speeds");
		// Serial.println(servoManager.getServoSpeeds());
	#endif // PRINT_SUMMARY

	// Send synchronization data via ESP-NOW
	synchronizer.send();
	#endif // BLUETOOTH_DEBUG_MODE
}
