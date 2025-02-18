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

#define RX_PIN 16
#define TX_PIN 17

#define LED_PIN         		10
#define LED_COUNT       	 	648

int frame = 0;
float updatesPerSecond = 30.0;

const float alpha_short = 1.0 / (30.0 * 5.0); // roughly 5 seconds
unsigned long lastUpdate = 0;

int brightness = 128;
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_RGBW + NEO_KHZ800);

LedColor ledColors[LED_COUNT];

ShaderManager shaderManager(strip, ledColors);

ServoManager servoManager;

// float spectrogram[NUM_BANDS] = { };
// float frequencies[SAMPLES / 2] = { };


void setup() {
	#if BLUETOOTH_DEBUG_MODE
	Serial.begin(115200);
	setupBluetooth();
	#else
	
	Serial.begin(115200);
	Serial1.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN);


	strip.begin();
	strip.clear();
	strip.show();
	strip.setBrightness(brightness);

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

	// computeSpectrogram(spectrogram);
	// doFFT(frequencies);
	float beatHeuristic = computeBeatHeuristic();

	servoManager.runServos();

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
	#endif // BLUETOOTH_DEBUG_MODE
}
