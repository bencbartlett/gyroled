#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <ESP32Servo.h>

#include "fft.h"
#include "beatdetection.h"
#include "bluetooth.h"
// #include "servos.hpp"
#include "shaders.hpp"


#define BLUETOOTH_DEBUG_MODE true

#define LED_PIN         32
#define SERVO_1_PIN     26
#define SERVO_2_PIN     27
#define SERVO_3_PIN     14
#define AUDIO_IN_PIN    35           
#define SERVO_UPDATE_HZ 50


#define LED_COUNT        648
#define LED_COUNT_RING_1 257
#define LED_COUNT_RING_2 212
#define LED_COUNT_RING_3 179
#define LED_COUNT_TOTAL  648

#define BRIGHTNESS 120

#define AMPLITUDE       1000          // Depending on your audio source level, you may need to alter this value. Can be used as a 'sensitivity' control.
#define NUM_BANDS       8             // To change this, you will need to change the bunch of if statements describing the mapping from bins to bands
#define NUM_BANDS_MSQEQ7 7
#define NOISE           20           // Used as a crude noise filter, values below this are ignored
#define PEAK_DECAY_RATE 5
#define NOISE_EMA_ALPHA 0.02        // At about 25fps this is about a 2 second window
#define RUN_LEDS true
#define RUN_SERVOS true
#define OUTPUT_TO_VISUALIZER true
#define USE_MSGEQ7 false

float avgNoise = 10.0;

// float servo_master_speed = 0.3; // Can range from 0 to 1
// float servo1_speed = 0.7; // Can range from -1 to 1
// float servo2_speed = 0.8;
// float servo3_speed = 1.0;

int ring_1_midpoint_L = (int)(LED_COUNT_RING_1 * 0.25);
int ring_1_midpoint_R = (int)(LED_COUNT_RING_1 * 0.75 + 1); // fudge factor
int ring_2_midpoint_L = (int)(LED_COUNT_RING_2 * 0.25 + LED_COUNT_RING_1);
int ring_2_midpoint_R = (int)(LED_COUNT_RING_2 * 0.75 + LED_COUNT_RING_1 - 1); // fudge factor
int ring_3_midpoint_L = (int)(LED_COUNT_RING_3 * 0.25 + LED_COUNT_RING_2 + LED_COUNT_RING_1);
int ring_3_midpoint_R = (int)(LED_COUNT_RING_3 * 0.75 + LED_COUNT_RING_2 + LED_COUNT_RING_1);

float peak[NUM_BANDS] = { };              // The length of these arrays must be >= NUM_BANDS
float spectrogram[NUM_BANDS] = { };

int noiseLevel = 0;
int frame = 0;

float updatesPerSecond = 0.0;

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_RGBW + NEO_KHZ800);
ShaderManager shaderManager(strip);

// ServoManager servoManager;

// Servo servo1;
// Servo servo2;
// Servo servo3;


// bool updateServo = true; // Flag to control servo update
// unsigned long lastServoUpdate = 0;
unsigned long lastSample = 0;
unsigned long lastUpdate = 0;


void setup() {

#if BLUETOOTH_DEBUG_MODE
	Serial.begin(115200);
	setupBluetooth();
#else

	Serial.begin(115200);

	strip.begin();
	strip.clear();
	strip.show();
	strip.setBrightness(BRIGHTNESS);

	// ESP32PWM::allocateTimer(0);
	// ESP32PWM::allocateTimer(1);
	// ESP32PWM::allocateTimer(2);
	// ESP32PWM::allocateTimer(3);
	// servo1.setPeriodHertz(50);    // standard 50 hz servo
	// servo2.setPeriodHertz(50);
	// servo3.setPeriodHertz(50);

	// servo1.attach(SERVO_1_PIN, 500, 2400);
	// servo2.attach(SERVO_2_PIN, 500, 2400);
	// servo3.attach(SERVO_3_PIN, 500, 2400);

	// servoManager.setupServos();
	setupAsyncSampling(); // Setup the ADC for async sampling

	// Reset bandValues[]
	for (int i = 0; i < NUM_BANDS; i++) {
		// bandValues[i] = 0;
		peak[i] = 0.;
		// oldBarHeights[i] = 0;
	}

	setupBluetooth();

#if USE_MSGEQ7
	setup_MSGEQ7();
	// setup_MSGEQ7_v2();
#endif

#if false
	setupI2S();
#endif

#endif // BLUETOOTH_DEBUG_MODE

}


/* =====================================================================================
 * COLOR PATTERNS WHEEEEE
 * =====================================================================================
 */



int clip255(int value) {
	// Ensure the value is not less than minVal and not greater than maxVal
	return std::min(std::max(value, 0), 255);
}



void applyWhiteBars(int noiseLevel) {
	int factor = 0.95;
	int amp = noiseLevel - avgNoise * factor > 0 ? (noiseLevel - avgNoise * factor) * 1 : 0;
	if (amp > 0) {
		strip.fill(strip.Color(255, 255, 255, 255), ring_1_midpoint_L - amp, 2 * amp);
		strip.fill(strip.Color(255, 255, 255, 255), ring_1_midpoint_R - amp, 2 * amp);
		strip.fill(strip.Color(255, 255, 255, 255), ring_2_midpoint_L - amp, 2 * amp);
		strip.fill(strip.Color(255, 255, 255, 255), ring_2_midpoint_R - amp, 2 * amp);
		strip.fill(strip.Color(255, 255, 255, 255), ring_3_midpoint_L - amp, 2 * amp);
		strip.fill(strip.Color(255, 255, 255, 255), ring_3_midpoint_R - amp, 2 * amp);
	}
}


void pulseWhiteAndRGB() {
	int wait = 5;
	for (int j = 0; j < 256; j++) { // Ramp up from 0 to 255
		// Fill entire strip with white at gamma-corrected brightness level 'j':
		strip.fill(strip.Color(strip.gamma8(j), strip.gamma8(j), strip.gamma8(j), strip.gamma8(j)));
		strip.show();
		delay(wait);
	}

	for (int j = 255; j >= 0; j--) { // Ramp down from 255 to 0
		strip.fill(strip.Color(strip.gamma8(j), strip.gamma8(j), strip.gamma8(j), strip.gamma8(j)));
		strip.show();
		delay(wait);
	}
}


void colorWipe(uint32_t color, int wait) {
	for (int i = 0; i < strip.numPixels(); i++) { // For each pixel in strip...
		strip.setPixelColor(i, color);         //  Set pixel's color (in RAM)
		strip.show();                          //  Update strip to match
		delay(wait);                           //  Pause for a moment
	}
}



void loop() {
#if BLUETOOTH_DEBUG_MODE
	delay(500);
	Serial.println(receivedValue);  // Use the receivedValue from bluetooth.cpp
#else
	delay(1);

	frame++;

	Serial.print("Received Value: ");
	Serial.println(receivedValue);  // Use the receivedValue from bluetooth.cpp


	updatesPerSecond = 1000.0 / float(millis() - lastUpdate);
	lastUpdate = millis();
	Serial.print("Updates per second: ");
	Serial.println(updatesPerSecond);

	lastSample = millis();
	computeSpectrogram(spectrogram);
	// Process the FFT data into bar heights
	for (int band = 0; band < NUM_BANDS; band++) {
		// Move peak up
		if (spectrogram[band] > peak[band]) {
			// peak[band] = min(TOP, barHeight);
			peak[band] = spectrogram[band];
		}
	}

	noiseLevel = (peak[0] + peak[1] + peak[2] + peak[3]);
	avgNoise = (NOISE_EMA_ALPHA * noiseLevel) + ((1 - NOISE_EMA_ALPHA) * avgNoise);
	// noiseLevel = (oldBarHeights[1] + oldBarHeights[2] + oldBarHeights[3] + oldBarHeights[4]); // / (AMPLITUDE / 4);

	// Decay peak
	for (byte band = 0; band < NUM_BANDS; band++) {
		if (peak[band] > 0) peak[band] -= PEAK_DECAY_RATE;
	}

	float beatHeuristic = computeBeatHeuristic(spectrogram);

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

#if RUN_SERVOS
	// servoManager.runServos();
#endif // RUN_SERVOS

#if RUN_LEDS

	shaderManager.activeShader->update(frame);
	applyWhiteBars(noiseLevel);

	strip.show();
#endif // RUN_LEDS

#endif // BLUETOOTH_DEBUG_MODE
}
