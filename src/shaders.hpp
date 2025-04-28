#include <Arduino.h>
#include <map>
#include <vector>
#include <Adafruit_NeoPixel.h>
#include <cmath>

#define NUM_RINGS 6

const int led_counts_outside[NUM_RINGS] = {
	56,
	40,
	40,
	40,
	40,
	40
};
const int led_counts_inside[NUM_RINGS] = {
	54,
	40,
	40,
	40,
	40,
	40
};

// Wastes a few bytes of ram but makes import ordering much nicer
const int MAX_LED_PER_RING = 56; // led_counts_outside[0] > led_counts_inside[0] ? led_counts_outside[0] : led_counts_inside[0];

extern int led_count_this_ring; // this is populated in ShaderManager(); can't be done statically because of import ordering
extern int led_count_this_ring_inside;

const int led_count_total_outside = led_counts_outside[0] + led_counts_outside[1] + led_counts_outside[2] + led_counts_outside[3] + led_counts_outside[4] + led_counts_outside[5];
const int led_count_total_inside = led_counts_inside[0] + led_counts_inside[1] + led_counts_inside[2] + led_counts_inside[3] + led_counts_inside[4] + led_counts_inside[5];
const int led_count_total = led_count_total_outside + led_count_total_inside;

// TODO: fix
// const int ring_1_midpoint_L = (int)(LED_COUNT_RING_1 * 0.25);
// const int ring_1_midpoint_R = (int)(LED_COUNT_RING_1 * 0.75 + 1); // fudge factor
// const int ring_2_midpoint_U = (int)(LED_COUNT_RING_2 * 0.25 + LED_COUNT_RING_1);
// const int ring_2_midpoint_D = (int)(LED_COUNT_RING_2 * 0.75 + LED_COUNT_RING_1 - 1); // fudge factor
// const int ring_3_midpoint_L = (int)(LED_COUNT_RING_3 * 0.25 + LED_COUNT_RING_2 + LED_COUNT_RING_1);
// const int ring_3_midpoint_R = (int)(LED_COUNT_RING_3 * 0.75 + LED_COUNT_RING_2 + LED_COUNT_RING_1);

extern unsigned long lastBeatTimestamp;  // Defined in beatdetection.cpp
extern unsigned long elapsedBeats;
extern int deviceIndex;

struct LedColor {
	uint8_t r, g, b, w;
	LedColor(
		uint8_t red = 0,
		uint8_t green = 0,
		uint8_t blue = 0,
		uint8_t white = 0
	) : r(red), g(green), b(blue), w(white) {}
	LedColor(uint32_t color) {
		w = (color >> 24) & 0xFF;
		r = (color >> 16) & 0xFF;
		g = (color >> 8) & 0xFF;
		b = color & 0xFF;
	}
	uint32_t pack() const {
		return Adafruit_NeoPixel::Color(g, r, b, w);  // seems like it always uses GRBW order regardless of what I tell it
	}

	static LedColor interpolate(const LedColor& color1, const LedColor& color2, float t) {
		if (t < 0.0f) t = 0.0f;
		if (t > 1.0f) t = 1.0f;

		return LedColor(
			static_cast<uint8_t>(int(color1.r) + (int(int(color2.r) - int(color1.r)) * t)),
			static_cast<uint8_t>(int(color1.g) + (int(int(color2.g) - int(color1.g)) * t)),
			static_cast<uint8_t>(int(color1.b) + (int(int(color2.b) - int(color1.b)) * t)),
			static_cast<uint8_t>(int(color1.w) + (int(int(color2.w) - int(color1.w)) * t))
		);
	}

	/* Interpolates zig-zagging back and forth, so at 0 it is color1, at 1 it is color2, then at 2 it is color1 again. */
	static LedColor interpolateZigZag(const LedColor& color1, const LedColor& color2, float t) {
		float t_mod;
		int period = static_cast<int>(floor(t));
		float fractional = t - period;
		if (period % 2 == 0) {
			t_mod = fractional;  // Interpolates from 0 to 1
		}
		else {
			t_mod = 1.0f - fractional;  // Interpolates from 1 to 0
		}
		return LedColor::interpolate(color1, color2, t_mod);
	}

	static LedColor hueInterpolate(float t, int startHue, int endHue) {
		if (endHue < startHue) {
			endHue += 360;
		}
		uint16_t hue = map(int(t * 65536) % 65536, 0, 65536, startHue, endHue);
		hue = hue % 360;
		return LedColor(Adafruit_NeoPixel::gamma32(Adafruit_NeoPixel::ColorHSV(hue * 182)));  // Multiply by 182 to convert 0-360 to 0-65535
	}

	static LedColor hueInterpolateZigZag(float t, int startHue, int endHue) {
		float t_mod;
		int period = static_cast<int>(floor(t));
		float fractional = t - period;
		if (period % 2 == 0) {
			t_mod = fractional;  // Interpolates from 0 to 1	
		}
		else {
			t_mod = 1.0f - fractional;  // Interpolates from 1 to 0
		}
		return LedColor::hueInterpolate(t_mod, startHue, endHue);
	}

	static LedColor hueInterpolateCCW(float t, int startHue, int endHue) {
		if (startHue < endHue) {
			startHue += 360;
		}
		uint16_t hue = map(int(t * 65536) % 65536, 0, 65536, startHue, endHue);
		hue = hue % 360;
		return LedColor(Adafruit_NeoPixel::gamma32(Adafruit_NeoPixel::ColorHSV(hue * 182)));  // Multiply by 182 to convert 0-360 to 0-65535
	}
};

/**
 * LED helper functions
 */

// Returns the x position of a LED on any ring, normalized between -1 and +1
inline float getXpos(int ledIndex, int ledCount) {
	// Rings are oriented in alternating x and y axes. 
	// deviceIndex % 2 == 0 spins about x axis, deviceIndex % 2 == 1 spins about y axis
	if (deviceIndex % 2 == 0) {
		return -1.0 * cos(2 * PI * ledIndex / ledCount);
	} else {
		return sin(2 * PI * ledIndex / ledCount);
	}
}

// Returns the y position of a LED on any ring, normalized between -1 and +1
inline float getYpos(int ledIndex, int ledCount) {
	if (deviceIndex % 2 == 0) {
		return -1.0 * sin(2 * PI * ledIndex / ledCount);
	} else {
		return -1.0 * cos(2 * PI * ledIndex / ledCount);
	}
}


/**
 * Filters
 */

inline LedColor sinLoops(LedColor inputColor, float theta, int power = 4) {
	float sineVal = sin(theta);
	float amplitude = (1 - pow(sineVal, power));
	LedColor color(
		inputColor.r * amplitude,
		inputColor.g * amplitude,
		inputColor.b * amplitude,
		inputColor.w * amplitude
	);
	return color;
}

inline LedColor squareLoops(LedColor inputColor, float theta) {
	float squareVal = theta > PI ? 1 : 0;
	LedColor color(
		inputColor.r * squareVal,
		inputColor.g * squareVal,
		inputColor.b * squareVal,
		inputColor.w * squareVal
	);
	return color;
}


/**
 * Shaders
 */

class Shader {
protected:
	// Adafruit_NeoPixel& strip;
	LedColor(&ledColors)[MAX_LED_PER_RING];
	String name;
	int ledCount;
	// Helper methods
	void fill(LedColor color, int start, int length) {
		for (int i = start; i < start + length; i++) {
			ledColors[i] = color;
		}
	}
public:
	Shader(LedColor(&colors)[MAX_LED_PER_RING], String shaderName, int ledCount) : ledColors(colors), name(shaderName), ledCount(ledCount) {}
	virtual void update(int frame) = 0;  // Pure virtual function to be implemented by each shader
	String getName() const {
		return name;
	}
};

/*
class WhiteOverRainbow : public Shader {
private:
	int head = 10;
	int tail = -1; // Start outside of any valid range
	int loops = 3;
	int loopNum = 0;
	uint32_t lastTime = 0;
	uint32_t firstPixelHue = 0;
	const int whiteSpeed = 5; // Speed of white light moving
	const int whiteLength = 30; // Length of the white light
public:
	WhiteOverRainbow(LedColor(&colors)[MAX_LED_PER_RING]) : Shader(colors, "White Over Rainbow") {}
	void update(int frame) override {
		if (whiteLength >= MAX_LED_PER_RING) return; // Error handling

		for (int i = 0; i < LED_COUNT_TOTAL; i++) {
			if (((i >= tail) && (i <= head)) || ((tail > head) && ((i >= tail) || (i <= head)))) {
				ledColors[i] = LedColor(255, 255, 255, 255); // Set white
			}
			else {
				int pixelHue = firstPixelHue + (i * 65536L / LED_COUNT_TOTAL);
				ledColors[i] = LedColor(Adafruit_NeoPixel::gamma32(Adafruit_NeoPixel::ColorHSV(pixelHue)));
			}
		}

		firstPixelHue += 40;

		if ((millis() - lastTime) > whiteSpeed) {
			if (++head >= LED_COUNT_TOTAL) {
				head = 0;
				if (++loopNum >= loops) loopNum = 0;
			}
			if (++tail >= LED_COUNT_TOTAL) {
				tail = 0;
			}
			lastTime = millis();
		}
	}
};

class LoopyRainbow : public Shader {
private:
	int cycleSpeed = 1000;
	int fadeVal = 100;
	int fadeMax = 100;
public:
	LoopyRainbow(LedColor(&colors)[LED_COUNT_TOTAL]) : Shader(colors, "Loopy Rainbow") {}
	void update(int frame) override {
		for (int i = 0; i < LED_COUNT_TOTAL; i++) {
			uint32_t pixelHue = frame * cycleSpeed + (i * 65536L / LED_COUNT_TOTAL);
			ledColors[i] = LedColor(Adafruit_NeoPixel::gamma32(Adafruit_NeoPixel::ColorHSV(pixelHue, 255, 255 * fadeVal / fadeMax)));
		}
	}
};

class LoopyRainbow2 : public Shader {
private:
	int cycleSpeed1 = 500;
	int cycleSpeed2 = 1000;
	int cycleSpeed3 = 2000;
	int fadeVal = 100;
	int fadeMax = 100;
public:
	LoopyRainbow2(LedColor(&colors)[LED_COUNT_TOTAL]) : Shader(colors, "Loopy Rainbow 2") {}
	void update(int frame) override {
		for (int i = 0; i < LED_COUNT_RING_1; i++) {
			uint32_t pixelHue = frame * cycleSpeed1 + (i * 65536L / LED_COUNT_TOTAL);
			ledColors[i] = LedColor(Adafruit_NeoPixel::gamma32(Adafruit_NeoPixel::ColorHSV(pixelHue, 255, 255 * fadeVal / fadeMax)));
		}
		for (int i = 0; i < LED_COUNT_RING_2; i++) {
			uint32_t pixelHue = frame * cycleSpeed2 + (i * 65536L / LED_COUNT_TOTAL);
			ledColors[i + LED_COUNT_RING_1] = LedColor(Adafruit_NeoPixel::gamma32(Adafruit_NeoPixel::ColorHSV(pixelHue, 255, 255 * fadeVal / fadeMax)));
		}
		for (int i = 0; i < LED_COUNT_RING_3; i++) {
			uint32_t pixelHue = frame * cycleSpeed3 + (i * 65536L / LED_COUNT_TOTAL);
			ledColors[i + LED_COUNT_RING_1 + LED_COUNT_RING_2] = LedColor(Adafruit_NeoPixel::gamma32(Adafruit_NeoPixel::ColorHSV(pixelHue, 255, 255 * fadeVal / fadeMax)));
		}
	}
};
*/



class Inferno : public Shader {
private:
	int cycleTime = 100;  // Determines how quickly the colors cycle
	int periods = 2;

	// Define hue ranges for each ring. Values are in degrees (0-360) on the color wheel.
	struct HueRange {
		uint16_t startHue;
		uint16_t endHue;
	};

	HueRange ringHueRanges[NUM_RINGS] = {
		{230, 280},  // Ring 1: Blue to Purple (240 is blue, 270 is violet)
		{280, 320},  
		{290, 359},  // Ring 3: Purple to Magenta (270 is violet, 300 is magenta)
		{320, 359},
		{0, 20},
		{0, 40}    // Ring 3: Magenta to Red to Yellow (300 is magenta, 60 is yellow)
	};

public:
	Inferno(LedColor(&colors)[MAX_LED_PER_RING], int ledCount) : Shader(colors, "Inferno", ledCount) {}

	void update(int frame) override {
		for (int i = 0; i < ledCount; i++) {
			float t = float(2 * i) / float(ledCount) + float(frame) / float(cycleTime);
			ledColors[i] = LedColor::hueInterpolateZigZag(t, ringHueRanges[deviceIndex].startHue, ringHueRanges[deviceIndex].endHue);
			// ledColors[i] = hueInterpolate(i, ringHueRanges[deviceIndex], frame, ledCount);
			// ledColors[i] = LedColor::interpolateZigZag(ledColors[i], ledColors[i], float(i) / ledCount);
		}
	}

	LedColor sineHueInterpolate(int ledIndex, HueRange hueRange, int frame, int totalLeds) {
		float sinVal = 0.5 + 0.5 * sin(
			2.0 * PI * (
				(float(periods) * float(ledIndex) / float(totalLeds))
				+ (float(frame) / cycleTime)
			)
		);
		uint16_t hue = map(int(sinVal * 65536) % 65536, 0, 65536, hueRange.startHue, hueRange.endHue);
		return LedColor(Adafruit_NeoPixel::gamma32(Adafruit_NeoPixel::ColorHSV(hue * 182)));  // Multiply by 182 to convert 0-360 to 0-65535
	}
};

// class InfernoTest : public Shader {
// 	private:
// 		int cycleTime = 20;  // Determines how quickly the colors cycle
// 		int periods = 4;
	
// 		// Define hue ranges for each ring. Values are in degrees (0-360) on the color wheel.
// 		struct HueRange {
// 			uint16_t startHue;
// 			uint16_t endHue;
// 		};
	
// 		HueRange ringHueRange = {250, 40};  // Ring 1: Blue to Purple (240 is blue, 270 is violet)
	
// 	public:
// 		InfernoTest(LedColor(&colors)[LED_COUNT_TOTAL]) : Shader(colors, "InfernoTest") {}
	
// 		void update(int frame) override {
// 			for (int i = 0; i < LED_COUNT_TOTAL; i++) {
// 				float t = fmod((periods * float(i) / LED_COUNT_TOTAL + float(frame) / cycleTime), 1.0);
// 				ledColors[i] = LedColor::hueInterpolate(t, ringHueRange.startHue, ringHueRange.endHue);
// 			}
// 		}
// 	};

// class Inferno2 : public Shader {
// private:
// 	int cycleTime = 50;  // Determines how quickly the colors cycle
// 	int periods = 1;

// 	LedColor blue;
// 	LedColor purple;
// 	LedColor magenta;
// 	LedColor orange;
// 	LedColor brightOrange;
// 	LedColor yellow;
// public:
// 	Inferno2(LedColor(&colors)[LED_COUNT_TOTAL]) : Shader(colors, "Inferno2"),
// 		blue(15, 7, 136, 0),
// 		purple(156, 24, 157, 0),
// 		magenta(230, 22, 98, 0),
// 		orange(228, 91, 47, 0),
// 		brightOrange(243, 118, 25, 0),
// 		yellow(245, 233, 37, 0) {};

// 	void update(int frame) override {
// 		for (int i = 0; i < LED_COUNT_RING_1; i++) {
// 			ledColors[i] = LedColor::interpolateZigZag(blue, purple, float(2 * periods * i) / LED_COUNT_RING_1);
// 			// ledColors[i] = getColorForLed(i, ringHueRanges[0], frame, LED_COUNT_RING_1);
// 		}
// 		for (int i = 0; i < LED_COUNT_RING_2; i++) {
// 			ledColors[i + LED_COUNT_RING_1] = LedColor::interpolateZigZag(magenta, orange, float(2 * periods * i) / LED_COUNT_RING_2);
// 			// ledColors[i + LED_COUNT_RING_1] = getColorForLed(i, ringHueRanges[1], frame, LED_COUNT_RING_2);
// 		}
// 		for (int i = 0; i < LED_COUNT_RING_3; i++) {
// 			ledColors[i + LED_COUNT_RING_1 + LED_COUNT_RING_2] = LedColor::interpolateZigZag(brightOrange, yellow, float(2 * periods * i) / LED_COUNT_RING_3);
// 			// ledColors[i + LED_COUNT_RING_1 + LED_COUNT_RING_2] = getColorForLed(i, ringHueRanges[2], frame, LED_COUNT_RING_3);
// 		}
// 	}
// };

// class BluePurple : public Shader {
// private:
// 	int cycleTime = 50;  // Determines how quickly the colors cycle
// 	int periods = 1;

// 	LedColor cyan;
// 	LedColor blue;
// 	LedColor purple;
// 	LedColor magenta;
// public:
// 	BluePurple(LedColor(&colors)[LED_COUNT_TOTAL]) : Shader(colors, "Blue Purple"),
// 		cyan(0, 200, 200, 0),
// 		blue(0, 20, 255, 0),
// 		purple(156, 0, 255, 0),
// 		magenta(255, 0, 255, 0) {};

// 	void update(int frame) override {
// 		for (int i = 0; i < LED_COUNT_RING_1; i++) {
// 			ledColors[i] = LedColor::interpolateZigZag(cyan, blue, float(2 * periods * i) / LED_COUNT_RING_1);
// 		}
// 		for (int i = 0; i < LED_COUNT_RING_2; i++) {
// 			ledColors[i + LED_COUNT_RING_1] = LedColor::interpolateZigZag(blue, purple, float(2 * periods * i) / LED_COUNT_RING_2);
// 		}
// 		for (int i = 0; i < LED_COUNT_RING_3; i++) {
// 			ledColors[i + LED_COUNT_RING_1 + LED_COUNT_RING_2] = LedColor::interpolateZigZag(purple, magenta, float(2 * periods * i) / LED_COUNT_RING_3);
// 		}
// 	}
// };

// class AquaColors : public Shader {
// private:
// 	int cycleTime = 30;  // Determines how quickly the colors cycle
// 	int periods = 3;

// 	// Define hue ranges for each ring. Values are in degrees (0-360) on the color wheel, then converted to 0-65535.
// 	struct HueRange {
// 		uint16_t startHue;
// 		uint16_t endHue;
// 	};

// 	HueRange ringHueRanges[3] = {
// 		{170, 200},  // Ring 1: Blues to Purples (170 is light blue, 270 is violet)
// 		{190, 220},  // Ring 2: Cyan Colors (180 is cyan, 210 is deeper cyan)
// 		{150, 180}   // Ring 3: Turquoise Colors (150 is soft turquoise, 180 is cyan)
// 	};

// public:
// 	AquaColors(LedColor(&colors)[LED_COUNT_TOTAL]) : Shader(colors, "Aqua Colors") {}

// 	void update(int frame) override {
// 		for (int i = 0; i < LED_COUNT_RING_1; i++) {
// 			ledColors[i] = hueInterpolate(i, ringHueRanges[0], frame, LED_COUNT_RING_1);
// 		}
// 		for (int i = 0; i < LED_COUNT_RING_2; i++) {
// 			ledColors[i + LED_COUNT_RING_1] = hueInterpolate(i, ringHueRanges[1], frame, LED_COUNT_RING_2);
// 		}
// 		for (int i = 0; i < LED_COUNT_RING_3; i++) {
// 			ledColors[i + LED_COUNT_RING_1 + LED_COUNT_RING_2] = hueInterpolate(i, ringHueRanges[2], frame, LED_COUNT_RING_3);
// 		}
// 	}

// 	// LedColor getColorForLed(int ledIndex, HueRange hueRange, int frame, int totalLeds) {
// 	// 	uint16_t hue = map(int((float(frame) / cycleTime + .5 * sin(2 * PI * periods * float(ledIndex) / totalLeds)) * 65536) % 65536,
// 	// 		0, 65536, hueRange.startHue, hueRange.endHue);
// 	// 	return LedColor(Adafruit_NeoPixel::gamma32(Adafruit_NeoPixel::ColorHSV(hue * 182)));  // Multiply by 182 to convert 0-360 to 0-65535
// 	// }

// 	LedColor hueInterpolate(int ledIndex, HueRange hueRange, int frame, int totalLeds) {
// 		float sinVal = 0.5 + 0.5 * sin(
// 			2.0 * PI * (
// 				(float(periods) * float(ledIndex) / float(totalLeds))
// 				+ (float(frame) / cycleTime)
// 				)
// 		);
// 		uint16_t hue = map(int(sinVal * 65536) % 65536, 0, 65536, hueRange.startHue, hueRange.endHue);
// 		return LedColor(Adafruit_NeoPixel::gamma32(Adafruit_NeoPixel::ColorHSV(hue * 182)));  // Multiply by 182 to convert 0-360 to 0-65535
// 	}
// };


class RedSineWave : public Shader {
private:
	int periodsPerRing = 3;
	int p = 2;
	float speed = 0.015;
public:
	RedSineWave(LedColor(&colors)[MAX_LED_PER_RING], int ledCount) : Shader(colors, "Red Sine Waves", ledCount) {}
	void update(int frame) override {
		for (int i = 0; i < ledCount; i++) {
			float theta = 2 * PI * (float(i) / ledCount + frame * speed * .7);
			ledColors[i] = sinLoops(LedColor(255, 0, 0, 0), theta * periodsPerRing, p);
		}
	}
};


// class RedSquareWave : public Shader {
// private:
// 	int periodsPerRing = 3;
// 	int p = 4;
// 	float speed = 0.01;
// public:
// 	RedSquareWave(LedColor(&colors)[LED_COUNT_TOTAL]) : Shader(colors, "Red Square Wave") {}
// 	void update(int frame) override {
// 		for (int i = 0; i < LED_COUNT_RING_1; i++) {
// 			float theta = 2 * PI * (float(i) / LED_COUNT_RING_1 + frame * speed);
// 			ledColors[i] = squareLoops(LedColor(255, 0, 0, 0), theta * periodsPerRing);
// 		}
// 		for (int i = 0; i < LED_COUNT_RING_2; i++) {
// 			float theta = 2 * PI * (float(i) / LED_COUNT_RING_2 + frame * speed);
// 			ledColors[i + LED_COUNT_RING_1] = squareLoops(LedColor(255, 15, 0, 0), theta * periodsPerRing);
// 		}
// 		for (int i = 0; i < LED_COUNT_RING_3; i++) {
// 			float theta = 2 * PI * (float(i) / LED_COUNT_RING_3 + frame * speed);
// 			ledColors[i + LED_COUNT_RING_1 + LED_COUNT_RING_2] = squareLoops(LedColor(255, 30, 0, 0), theta * periodsPerRing);
// 		}
// 	}
// };


class Bisexual : public Shader {
private:
	float speed = 0.1;

	int startHue = 200;
	int endHue = 340;

public:
	Bisexual(LedColor(&colors)[MAX_LED_PER_RING], int ledCount) : Shader(colors, "Bisexual", ledCount) {}
	void update(int frame) override {
		for (int i = 0; i < ledCount; i++) {
			float x = getXpos(i, ledCount);
			float t = pow(sin(PI / 2 * x - speed * float(frame)), 2);
			ledColors[i] = LedColor::hueInterpolate(t, startHue, endHue);
		}
	}
}; 


class AccentShader {
protected:
	// Adafruit_NeoPixel& strip;
	LedColor(&ledColors)[MAX_LED_PER_RING];
	String name;
	int ledCount;
	// Helper methods
	void fill(LedColor color, int start, int length) {
		for (int i = start; i < start + length; i++) {
			ledColors[i] = color;
		}
	}
public:
	AccentShader(LedColor(&colors)[MAX_LED_PER_RING], String shaderName, int ledCount) : ledColors(colors), name(shaderName), ledCount(ledCount) {}
	virtual void update(int frame, float intensity) = 0;
	String getName() const {
		return name;
	}
};

class NoAccent : public AccentShader {
private:
public:
	NoAccent(LedColor(&colors)[MAX_LED_PER_RING], int ledCount) : AccentShader(colors, "(No Accent)", ledCount) {}
	void update(int frame, float intensity) override { }
};

// class WhitePeaks : public AccentShader {
// private:
// 	float factor = 0.07;
// 	float maxWhiteAmount = 0.45;
// 	float minWhiteCutoff = 0.05;
// 	bool usePureWhite = true;
// 	float peak = 0.0;
// 	float falloff = 1.0 / (30.0 * 0.25); // 0.25 second falloff
// public:
// 	WhitePeaks(LedColor(&colors)[LED_COUNT_TOTAL]) : AccentShader(colors, "White Peaks") {}
// 	void update(int frame, float intensity) override {
// 		peak = std::max(std::min(std::min(factor * intensity, float(1)), maxWhiteAmount), peak);
// 		if (peak < minWhiteCutoff) {
// 			return;
// 		}
// 		int amp1 = peak * LED_COUNT_RING_1 / 4;
// 		int amp2 = peak * LED_COUNT_RING_2 / 4;
// 		int amp3 = peak * LED_COUNT_RING_3 / 4;
// 		// Serial.println(amp1);
// 		// Serial.println(amp2);
// 		// Serial.println(amp3);
// 		if (intensity > 0) {
// 			if (usePureWhite) {
// 				int ring_2_start = LED_COUNT_RING_1;
// 				int ring_2_end = LED_COUNT_RING_1 + LED_COUNT_RING_2;
// 				int ring_2_middle = (ring_2_start + ring_2_end) / 2;
// 				fill(LedColor(255, 255, 255, 255), ring_1_midpoint_L - amp1, 2 * amp1);
// 				fill(LedColor(255, 255, 255, 255), ring_1_midpoint_R - amp1, 2 * amp1);
// 				fill(LedColor(255, 255, 255, 255), ring_2_start, amp2);
// 				fill(LedColor(255, 255, 255, 255), ring_2_end - amp2, amp2);
// 				fill(LedColor(255, 255, 255, 255), ring_2_middle - amp2, 2 * amp2);
// 				fill(LedColor(255, 255, 255, 255), ring_3_midpoint_L - amp3, 2 * amp3);
// 				fill(LedColor(255, 255, 255, 255), ring_3_midpoint_R - amp3, 2 * amp3);
// 			}
// 			else {
// 				for (int i = ring_1_midpoint_L - amp1; i < ring_1_midpoint_L + amp1; i++) { ledColors[i].w = 255; }
// 				for (int i = ring_1_midpoint_R - amp1; i < ring_1_midpoint_R + amp1; i++) { ledColors[i].w = 255; }
// 				for (int i = ring_2_midpoint_U - amp2; i < ring_2_midpoint_U + amp2; i++) { ledColors[i].w = 255; }
// 				for (int i = ring_2_midpoint_D - amp2; i < ring_2_midpoint_D + amp2; i++) { ledColors[i].w = 255; }
// 				for (int i = ring_3_midpoint_L - amp3; i < ring_3_midpoint_L + amp3; i++) { ledColors[i].w = 255; }
// 				for (int i = ring_3_midpoint_R - amp3; i < ring_3_midpoint_R + amp3; i++) { ledColors[i].w = 255; }
// 			}
// 		}
// 		peak -= falloff;
// 		// Serial.println("done");
// 	}
// };


// class WhitePeaksBeatsOnly : public AccentShader {
// private:
// 	float factor = 0.07;
// 	float maxWhiteAmount = 0.6;
// 	float minWhiteCutoff = 0.01;
// 	bool usePureWhite = true;
// 	float peak = 0.0;
// 	float falloff = 1.0 / (30.0 * 0.15); // 0.15 second falloff
// 	float lastBeatTimestamp_internal = 0;
// public:
// 	WhitePeaksBeatsOnly(LedColor(&colors)[LED_COUNT_TOTAL]) : AccentShader(colors, "White Peaks (Beats Only)") {}
// 	void update(int frame, float intensity) override {

// 		if (lastBeatTimestamp_internal != lastBeatTimestamp) {
// 			// peak = std::max(std::min(std::min(factor * intensity, float(1)), maxWhiteAmount), peak);
// 			peak = maxWhiteAmount;
// 			lastBeatTimestamp_internal = lastBeatTimestamp;
// 		}

// 		if (peak < minWhiteCutoff) {
// 			return;
// 		}
// 		int amp1 = peak * LED_COUNT_RING_1 / 4;
// 		int amp2 = peak * LED_COUNT_RING_2 / 4;
// 		int amp3 = peak * LED_COUNT_RING_3 / 4;
// 		// Serial.println(amp1);
// 		// Serial.println(amp2);
// 		// Serial.println(amp3);
// 		if (intensity > 0) {
// 			if (usePureWhite) {
// 				int ring_2_start = LED_COUNT_RING_1;
// 				int ring_2_end = LED_COUNT_RING_1 + LED_COUNT_RING_2;
// 				int ring_2_middle = (ring_2_start + ring_2_end) / 2;
// 				fill(LedColor(255, 255, 255, 255), ring_1_midpoint_L - amp1, 2 * amp1);
// 				fill(LedColor(255, 255, 255, 255), ring_1_midpoint_R - amp1, 2 * amp1);
// 				fill(LedColor(255, 255, 255, 255), ring_2_start, amp2);
// 				fill(LedColor(255, 255, 255, 255), ring_2_end - amp2, amp2);
// 				fill(LedColor(255, 255, 255, 255), ring_2_middle - amp2, 2 * amp2);
// 				fill(LedColor(255, 255, 255, 255), ring_3_midpoint_L - amp3, 2 * amp3);
// 				fill(LedColor(255, 255, 255, 255), ring_3_midpoint_R - amp3, 2 * amp3);
// 			}
// 			else {
// 				for (int i = ring_1_midpoint_L - amp1; i < ring_1_midpoint_L + amp1; i++) { ledColors[i].w = 255; }
// 				for (int i = ring_1_midpoint_R - amp1; i < ring_1_midpoint_R + amp1; i++) { ledColors[i].w = 255; }
// 				for (int i = ring_2_midpoint_U - amp2; i < ring_2_midpoint_U + amp2; i++) { ledColors[i].w = 255; }
// 				for (int i = ring_2_midpoint_D - amp2; i < ring_2_midpoint_D + amp2; i++) { ledColors[i].w = 255; }
// 				for (int i = ring_3_midpoint_L - amp3; i < ring_3_midpoint_L + amp3; i++) { ledColors[i].w = 255; }
// 				for (int i = ring_3_midpoint_R - amp3; i < ring_3_midpoint_R + amp3; i++) { ledColors[i].w = 255; }
// 			}
// 		}
// 		peak -= falloff;
// 		// Serial.println("done");
// 	}
// };

// class LightFanIn : public AccentShader {
// private:
// 	float animationTime = 350.0;
// 	float whiteFractionOfArc = 0.3;
// 	bool pulseOnAllRings = false;
// public:
// 	LightFanIn(LedColor(&colors)[LED_COUNT_TOTAL]) : AccentShader(colors, "Light Fan-In") {}
// 	void update(int frame, float intensity) override {

// 		float amp = float(millis() - lastBeatTimestamp) / animationTime;
// 		int ringIndex = elapsedBeats % 3;

// 		if (amp <= 1.0) {
// 			int amp1 = amp * LED_COUNT_RING_1 / 4;
// 			int amp2 = amp * LED_COUNT_RING_2 / 4;
// 			int amp3 = amp * LED_COUNT_RING_3 / 4;

// 			int white1 = whiteFractionOfArc * LED_COUNT_RING_1 / 4;
// 			int white2 = whiteFractionOfArc * LED_COUNT_RING_2 / 4;
// 			int white3 = whiteFractionOfArc * LED_COUNT_RING_3 / 4;

// 			if (amp + whiteFractionOfArc > 1) {
// 				white1 *= 1 - (amp + whiteFractionOfArc - 1) / whiteFractionOfArc;
// 				white2 *= 1 - (amp + whiteFractionOfArc - 1) / whiteFractionOfArc;
// 				white3 *= 1 - (amp + whiteFractionOfArc - 1) / whiteFractionOfArc;
// 			}

// 			int ring_2_start = LED_COUNT_RING_1;
// 			int ring_2_end = LED_COUNT_RING_1 + LED_COUNT_RING_2;
// 			int ring_2_middle = (ring_2_start + ring_2_end) / 2;

// 			if (pulseOnAllRings || ringIndex == 0) {
// 				fill(LedColor(255, 255, 255, 255), ring_1_midpoint_L + amp1, white1);
// 				fill(LedColor(255, 255, 255, 255), ring_1_midpoint_L - amp1 - white1, white1);
// 				fill(LedColor(255, 255, 255, 255), ring_1_midpoint_R + amp1, white1);
// 				fill(LedColor(255, 255, 255, 255), ring_1_midpoint_R - amp1 - white1, white1);
// 			}

// 			if (pulseOnAllRings || ringIndex == 1) {
// 				fill(LedColor(255, 255, 255, 255), ring_2_start + amp2, white2);
// 				fill(LedColor(255, 255, 255, 255), ring_2_end - amp2 - white2, white2);
// 				fill(LedColor(255, 255, 255, 255), ring_2_middle + amp2, white2);
// 				fill(LedColor(255, 255, 255, 255), ring_2_middle - amp2 - white2, white2);
// 			}

// 			if (pulseOnAllRings || ringIndex == 2) {
// 				fill(LedColor(255, 255, 255, 255), ring_3_midpoint_L + amp3, white3);
// 				fill(LedColor(255, 255, 255, 255), ring_3_midpoint_L - amp3 - white3, white3);
// 				fill(LedColor(255, 255, 255, 255), ring_3_midpoint_R + amp3, white3);
// 				fill(LedColor(255, 255, 255, 255), ring_3_midpoint_R - amp3 - white3, white3);
// 			}
// 		}
// 	}
// };

// class PulsedStrobeOverlay : public AccentShader {
// private:
// 	float brightnessScale = 0.25;
// public:
// 	PulsedStrobeOverlay(LedColor(&colors)[LED_COUNT_TOTAL]) : AccentShader(colors, "Pulsed Strobe Overlay") {}
// 	void update(int frame, float intensity) override {
// 		for (int i = 0; i < LED_COUNT_TOTAL; i++) {
// 			ledColors[i].w = uint8_t(255 * brightnessScale * intensity);
// 		}
// 	}
// };

// class PulseIntensity : public AccentShader {
// private:
// 	float minBrightnessScale = 0.25;
// 	float maxBrightnessScale = 1.0;
// 	float animationTime = 300.0;
// public:
// 	PulseIntensity(LedColor(&colors)[LED_COUNT_TOTAL]) : AccentShader(colors, "Pulsed Intensity") {}
// 	void update(int frame, float intensity) override {

// 		float prog = std::max(std::min(float(float(millis() - lastBeatTimestamp) / animationTime), float(0)), float(1));
// 		float brightness = maxBrightnessScale * prog + minBrightnessScale * (1-prog);

// 		for (int i = 0; i < LED_COUNT_TOTAL; i++) {
// 			ledColors[i].r = uint8_t(brightness * ledColors[i].r);
// 			ledColors[i].g = uint8_t(brightness * ledColors[i].g);
// 			ledColors[i].b = uint8_t(intensity * ledColors[i].b);
// 			ledColors[i].w = uint8_t(brightness * ledColors[i].w);
// 		}
// 	}
// };

// class Strobe : public AccentShader {
// private:
// 	float brightnessScale = 0.8;
// 	int strobeFrames = 15;
// public:
// 	Strobe(LedColor(&colors)[LED_COUNT_TOTAL]) : AccentShader(colors, "Strobe") {}
// 	void update(int frame, float intensity) override {
// 		if (frame % strobeFrames < strobeFrames / 2) {
// 			for (int i = 0; i < LED_COUNT_TOTAL; i++) {
// 				ledColors[i].w = uint8_t(255 * brightnessScale * intensity);
// 			}
// 		}
// 	}
// };

// class BeatHueShift : public AccentShader {
// private:
// 	float hueShiftFactor = 0.0002;
// 	float cutoff = 1.0;
// 	uint16_t phase = 0;  // Tracks the last hue shift to create a continuous effect
// public:
// 	BeatHueShift(LedColor(&colors)[LED_COUNT_TOTAL]) : AccentShader(colors, "Beat Hue Shift") {}

// 	void update(int frame, float intensity) override {
// 		// Calculate the hue shift as a 16-bit value (0 to 65535), proportional to intensity
// 		if (intensity < cutoff) {
// 			return;
// 		}
// 		uint16_t hueShift = (uint16_t)(intensity * intensity * hueShiftFactor * 65535) % 65535;
// 		phase = (phase + hueShift) % 65535;

// 		for (int i = 0; i < LED_COUNT_TOTAL; i++) {
// 			// Convert original RGB to its approximate HSV values for hue manipulation
// 			uint16_t hue;
// 			uint8_t sat, val;
// 			rgbToApproximateHsv(ledColors[i].r, ledColors[i].g, ledColors[i].b, hue, sat, val);
// 			// Apply hue shift
// 			hue = (hue + phase) % 65535;
// 			// Convert back to RGB using Adafruit's HSV to RGB conversion
// 			ledColors[i] = LedColor(Adafruit_NeoPixel::ColorHSV(hue, sat, val));
// 			ledColors[i].r *= constrain(.5 * (1 + intensity), 0., 1.);
// 			ledColors[i].g *= constrain(.5 * (1 + intensity), 0., 1.);
// 			ledColors[i].b *= constrain(.5 * (1 + intensity), 0., 1.);
// 			ledColors[i].w *= constrain(.5 * (1 + intensity), 0., 1.);
// 		}
// 	}

// 	// A simple approximation method to convert RGB to HSV
// 	// This is a placeholder for you to implement a more accurate conversion if needed
// 	void rgbToApproximateHsv(uint8_t r, uint8_t g, uint8_t b, uint16_t& hue, uint8_t& sat, uint8_t& val) {
// 		uint8_t min, max, delta;
// 		min = r < g ? (r < b ? r : b) : (g < b ? g : b);
// 		max = r > g ? (r > b ? r : b) : (g > b ? g : b);
// 		val = max;  // value is the maximum of r, g, b
// 		delta = max - min;
// 		if (delta == 0) { // gray, no color
// 			hue = 0;
// 			sat = 0;
// 		}
// 		else { // Colorful
// 			sat = 255 * delta / max;
// 			if (r == max)
// 				hue = (g - b) / (float)delta; // between yellow & magenta
// 			else if (g == max)
// 				hue = 2 + (b - r) / (float)delta; // between cyan & yellow
// 			else
// 				hue = 4 + (r - g) / (float)delta; // between magenta & cyan
// 			hue *= 60; // degrees
// 			if (hue < 0)
// 				hue += 360;
// 			hue = (uint16_t)(hue / 360.0 * 65535);  // Convert to 0-65535 range
// 		}
// 	}
// };

// class BeatHueShiftQuantized : public AccentShader {
// private:
// 	float hueShiftAngle = 121.0;
// 	float theta = 0.0;
// public:
// 	BeatHueShiftQuantized(LedColor(&colors)[LED_COUNT_TOTAL]) : AccentShader(colors, "Beat Hue Shift Quantized") {}

// 	void update(int frame, float intensity) override {
// 		float dTheta = 0.5;
// 		theta = fmod(theta + dTheta, 360.0);
// 		float angle = fmod(hueShiftAngle * (elapsedBeats % (360 / 30)), 360.0) + theta;
// 		uint16_t phase = uint16_t(fmod(angle, 360.) / 360.0 * 65535) % 65535;

// 		for (int i = 0; i < LED_COUNT_TOTAL; i++) {
// 			// Convert original RGB to its approximate HSV values for hue manipulation
// 			uint16_t hue;
// 			uint8_t sat, val;
// 			rgbToApproximateHsv(ledColors[i].r, ledColors[i].g, ledColors[i].b, hue, sat, val);
// 			// Apply hue shift
// 			hue = (hue + phase) % 65535;
// 			// Convert back to RGB using Adafruit's HSV to RGB conversion
// 			ledColors[i] = LedColor(Adafruit_NeoPixel::ColorHSV(hue, sat, val));
// 			ledColors[i].r *= constrain(.5 * (1 + intensity), 0., 1.);
// 			ledColors[i].g *= constrain(.5 * (1 + intensity), 0., 1.);
// 			ledColors[i].b *= constrain(.5 * (1 + intensity), 0., 1.);
// 			ledColors[i].w *= constrain(.5 * (1 + intensity), 0., 1.);
// 		}
// 	}

// 	// A simple approximation method to convert RGB to HSV
// 	// This is a placeholder for you to implement a more accurate conversion if needed
// 	void rgbToApproximateHsv(uint8_t r, uint8_t g, uint8_t b, uint16_t& hue, uint8_t& sat, uint8_t& val) {
// 		uint8_t min, max, delta;
// 		min = r < g ? (r < b ? r : b) : (g < b ? g : b);
// 		max = r > g ? (r > b ? r : b) : (g > b ? g : b);
// 		val = max;  // value is the maximum of r, g, b
// 		delta = max - min;
// 		if (delta == 0) { // gray, no color
// 			hue = 0;
// 			sat = 0;
// 		}
// 		else { // Colorful
// 			sat = 255 * delta / max;
// 			if (r == max)
// 				hue = (g - b) / (float)delta; // between yellow & magenta
// 			else if (g == max)
// 				hue = 2 + (b - r) / (float)delta; // between cyan & yellow
// 			else
// 				hue = 4 + (r - g) / (float)delta; // between magenta & cyan
// 			hue *= 60; // degrees
// 			if (hue < 0)
// 				hue += 360;
// 			hue = (uint16_t)(hue / 360.0 * 65535);  // Convert to 0-65535 range
// 		}
// 	}
// };



class ColorCounter : public Shader {
private:
	// Define the colors we'll cycle through
	const LedColor colorPalette[5] = {
		LedColor(255, 255, 255, 255),  // White
		LedColor(255, 0, 0, 0),        // Red
		LedColor(0, 255, 0, 0),        // Green
		LedColor(0, 0, 255, 0),        // Blue
		LedColor(255, 0, 255, 0)       // Purple
	};
	const int numColors = 5;

public:
	ColorCounter(LedColor(&colors)[MAX_LED_PER_RING], int ledCount) : Shader(colors, "Color Counter", ledCount) {}
	
	void update(int frame) override {
		for (int i = 0; i < ledCount; i++) {
			// Cycle through colors based on LED index
			int colorIndex = i % numColors;
			ledColors[i] = colorPalette[colorIndex];
		}
	}
};

class ShaderManager {
private:
	Adafruit_NeoPixel& strip_outside_cw;
	Adafruit_NeoPixel& strip_outside_ccw;
	Adafruit_NeoPixel& strip_inside_cw;
	LedColor ledColorsOutside[MAX_LED_PER_RING];
	LedColor ledColorsInside[MAX_LED_PER_RING];
	// unsigned long lastShaderChangeMs = 0;
public:
	bool hasPhoneEverConnected = false;
	bool useAnimation = true;
	bool animationHasBeenChanged = false;

	std::map<String, Shader*> shaders;
	std::map<String, AccentShader*> accentShaders;
	std::map<String, Shader*> shadersInside;
	std::map<String, AccentShader*> accentShadersInside;

	Shader* activeShader = nullptr;
	AccentShader* activeAccentShader = nullptr;
	Shader* activeShaderInside = nullptr;
	AccentShader* activeAccentShaderInside = nullptr;

	ShaderManager(
		Adafruit_NeoPixel& strip1, 
		Adafruit_NeoPixel& strip2, 
		Adafruit_NeoPixel& strip3 
	) : strip_outside_cw(strip1), strip_outside_ccw(strip2), strip_inside_cw(strip3) { }

	~ShaderManager() {
		// Clean up all shaders
		for (auto& shader : shaders) {
			delete shader.second;
		}
		shaders.clear();

		// Clean up all accent shaders
		for (auto& shader : accentShaders) {
			delete shader.second;
		}
		accentShaders.clear();

		// Clear the pointers but don't delete them as they're already deleted above
		activeShader = nullptr;
		activeAccentShader = nullptr;
		activeShaderInside = nullptr;
		activeAccentShaderInside = nullptr;
	}

	void init() {
		led_count_this_ring = led_counts_outside[deviceIndex];
		led_count_this_ring_inside = led_counts_inside[deviceIndex];

		Serial.println("LED led_count_this_ring: ");
		Serial.println(led_count_this_ring);

		Serial.println("LED led_count_this_ring_inside: ");
		Serial.println(led_count_this_ring_inside);
		Serial.println("LED deviceIndex: ");
		Serial.println(deviceIndex);
		Serial.println("--------------------------------");

		std::vector<Shader*> shaderList = {
			// good ones
			// new InfernoTest(ledColorsOutside, led_count_this_ring),
			new Inferno(ledColorsOutside, led_count_this_ring),
			new ColorCounter(ledColorsOutside, led_count_this_ring),
			new RedSineWave(ledColorsOutside, led_count_this_ring),
			new Bisexual(ledColorsOutside, led_count_this_ring),
			// new AquaColors(ledColorsOutside, led_count_this_ring),
			// new BluePurple(ledColorsOutside, led_count_this_ring),
			// new LoopyRainbow(ledColorsOutside, led_count_this_ring),
			// new LoopyRainbow2(ledColorsOutside, led_count_this_ring),
			// // shitty ones
			// new WhiteOverRainbow(ledColorsOutside, led_count_this_ring),
			// new Inferno2(ledColorsOutside, led_count_this_ring),
			// new RedSquareWave(ledColorsOutside, led_count_this_ring),
			// Add more shaders here
		};

		std::vector<Shader*> shaderListInside = {
			new Inferno(ledColorsInside, led_count_this_ring_inside),
			new ColorCounter(ledColorsInside, led_count_this_ring_inside),
			// good ones
			// new InfernoTest(ledColorsInside, led_count_this_ring_inside),
			new RedSineWave(ledColorsInside, led_count_this_ring_inside),
			// new AquaColors(ledColorsInside, led_count_this_ring_inside),
			// new BluePurple(ledColorsInside, led_count_this_ring_inside),
			// new LoopyRainbow(ledColorsInside, led_count_this_ring_inside),
			// new LoopyRainbow2(ledColorsInside, led_count_this_ring_inside),
			new Bisexual(ledColorsInside, led_count_this_ring_inside),
			// // shitty ones
			// new WhiteOverRainbow(ledColorsInside, led_count_this_ring_inside),
			// new Inferno2(ledColorsInside, led_count_this_ring_inside),
			// new RedSquareWave(ledColorsInside, led_count_this_ring_inside),
			// Add more shaders here
		};

		std::vector<AccentShader*> accentShaderList = {
			new NoAccent(ledColorsOutside, led_count_this_ring),
			// new WhitePeaks(ledColorsOutside, led_count_this_ring),
			// new LightFanIn(ledColorsOutside, led_count_this_ring),
			// new PulseIntensity(ledColorsOutside, led_count_this_ring),
			// new WhitePeaksBeatsOnly(ledColorsOutside, led_count_this_ring),
			// new PulsedStrobeOverlay(ledColorsOutside, led_count_this_ring),
			// new Strobe(ledColorsOutside, led_count_this_ring),
			// new BeatHueShift(ledColorsOutside, led_count_this_ring),
			// new BeatHueShiftQuantized(ledColorsOutside, led_count_this_ring),
			// Add more shaders here
		};

		std::vector<AccentShader*> accentShaderListInside = {
			new NoAccent(ledColorsInside, led_count_this_ring_inside),
			// new WhitePeaks(ledColorsInside, led_count_this_ring_inside),
			// new LightFanIn(ledColorsInside, led_count_this_ring_inside),
			// new PulseIntensity(ledColorsInside, led_count_this_ring_inside),
			// new WhitePeaksBeatsOnly(ledColorsInside, led_count_this_ring_inside),
			// new PulsedStrobeOverlay(ledColorsInside, led_count_this_ring_inside),
			// new Strobe(ledColorsInside, led_count_this_ring_inside),
			// new BeatHueShift(ledColorsInside, led_count_this_ring_inside),
			// new BeatHueShiftQuantized(ledColorsInside, led_count_this_ring_inside),
			// // Add more shaders here
		};

		for (Shader* shader : shaderList) {
			shaders[shader->getName()] = shader;
		}

		for (AccentShader* shader : accentShaderList) {
			accentShaders[shader->getName()] = shader;
		}

		for (Shader* shader : shaderListInside) {
			shadersInside[shader->getName()] = shader;
		}

		for (AccentShader* shader : accentShaderListInside) {
			accentShadersInside[shader->getName()] = shader;
		}

		if (!shaderList.empty()) {
			activeShader = shaderList[0]; // Set the first shader as the default active shader
			animationHasBeenChanged = true;
		}

		if (!accentShaderList.empty()) {
			activeAccentShader = accentShaderList[0]; // Set the first shader as the default active shader
		}

		if (!shaderListInside.empty()) {
			activeShaderInside = shaderListInside[0]; // Set the first shader as the default active shader
		}

		if (!accentShaderListInside.empty()) {
			activeAccentShaderInside = accentShaderListInside[0]; // Set the first shader as the default active shader
		}
	}
	void setupLedStrips(int brightness) {
		strip_outside_cw.begin();
		strip_outside_cw.setBrightness(brightness);
		strip_outside_cw.show();
		strip_outside_cw.clear();

		strip_outside_ccw.begin();
		strip_outside_ccw.setBrightness(brightness);
		strip_outside_ccw.show();
		strip_outside_ccw.clear();

		strip_inside_cw.begin();
		strip_inside_cw.setBrightness(brightness);
		strip_inside_cw.show();
		strip_inside_cw.clear();
	}

	void setBrightness(int brightness) {
		strip_outside_cw.setBrightness(brightness);
		strip_outside_ccw.setBrightness(brightness);
		strip_inside_cw.setBrightness(brightness);
	}

	void setActiveShader(const String& shaderName) {
		auto it = shaders.find(shaderName);
		if (it != shaders.end()) {
			activeShader = it->second;
			animationHasBeenChanged = true;
			// lastShaderChangeMs = millis();
		}
		else {
			Serial.println("Shader not found");
		}
	}

	void setActiveAccentShader(const String& shaderName) {
		auto it = accentShaders.find(shaderName);
		if (it != accentShaders.end()) {
			activeAccentShader = it->second;
			animationHasBeenChanged = true;
		}
		else {
			Serial.println("Accent Shader not found");
		}
	}

	void run(int frame, float intensity) {
		if (!useAnimation && !animationHasBeenChanged) {
			return;
		}

		// // If phone was never connected let's cycle through the good shaders
		// int tenMins = 30 * 60 * 10;
		// if (!hasPhoneEverConnected && frame % tenMins == tenMins - 1) {
		// 	// Change the shader
		// 	int changeFreq = 30 * 60 * 5; // 5 mins
		// 	int index = int(frame / changeFreq) % 5;
		// 	std::string goodShaderNames[5] = {
		// 		"Inferno",
		// 		"Red Sine Waves",
		// 		"Aqua Colors",
		// 		"Blue Purple",
		// 		"Loopy Rainbow"
		// 	};
		// 	setActiveShader(goodShaderNames[index].c_str());
		// }

		if (activeShader == nullptr) {
			Serial.println("Warning: No active shader set");
			return;
		}

		// Serial.println("LED strip_outside_cw.numPixels(): ");
		// Serial.println(strip_outside_cw.numPixels());

		// Serial.println("LED strip_outside_ccw.numPixels(): ");
		// Serial.println(strip_outside_ccw.numPixels());

		// Serial.println("LED strip_inside_cw.numPixels(): ");
		// Serial.println(strip_inside_cw.numPixels());

		// Serial.println("LED deviceIndex: ");
		// Serial.println(deviceIndex);

		// Serial.println("LED led_count_this_ring: ");
		// Serial.println(led_count_this_ring);

		// Serial.println("LED led_count_this_ring_inside: ");
		// Serial.println(led_count_this_ring_inside);


		activeShader->update(frame);
		activeShaderInside->update(frame);
		activeAccentShader->update(frame, intensity);
		activeAccentShaderInside->update(frame, intensity);

		// Flush the ledColors to the strip
		for (int i = 0; i < led_count_this_ring; i++) {
			strip_outside_cw.setPixelColor(i, ledColorsOutside[i].pack());
			strip_outside_ccw.setPixelColor(i, ledColorsOutside[(led_count_this_ring - 1) - i].pack()); // reverse in software
		}
		for (int i = 0; i < led_count_this_ring_inside; i++) {
			strip_inside_cw.setPixelColor(i, ledColorsInside[i].pack());
		}

		strip_outside_ccw.show();
		strip_outside_cw.show();
		strip_inside_cw.show();

		animationHasBeenChanged = false;
	}

};
