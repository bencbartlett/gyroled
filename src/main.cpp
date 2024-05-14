#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <ESP32Servo.h>

#include "fft.h"
#include "beatdetection.h"
#include "bluetooth.h"
#include "servos.hpp"
#include "shaders.hpp"


#define BLUETOOTH_DEBUG_MODE 	false
#define PRINT_SUMMARY    		false
#define OUTPUT_TO_VISUALIZER 	false

#define LED_PIN         		32
#define LED_COUNT       	 	648

int frame = 0;
float updatesPerSecond = 0.0;
unsigned long lastUpdate = 0;

int brightness = 255;
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_RGBW + NEO_KHZ800);

LedColor ledColors[LED_COUNT];

ShaderManager shaderManager(strip, ledColors);

ServoManager servoManager;

float spectrogram[NUM_BANDS] = { };


void setup() {
	#if BLUETOOTH_DEBUG_MODE
	Serial.begin(115200);
	setupBluetooth();
	#else
	Serial.begin(115200);
	shaderManager.setupStrip();
	servoManager.setupServos();
	setupAsyncSampling();
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
		strip.setBrightness(brightness);
	}

	computeSpectrogram(spectrogram);
	float beatHeuristic = computeBeatHeuristic(spectrogram);

	servoManager.runServos();

	shaderManager.run(frame, beatHeuristic);

	updatesPerSecond = 1000.0 / float(millis() - lastUpdate);
	lastUpdate = millis();

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
	#endif // BLUETOOTH_DEBUG_MODE
}
