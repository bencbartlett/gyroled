#include <map>
#include <vector>
#include <Adafruit_NeoPixel.h>
#include <cmath>

// #define LED_COUNT        648
#define LED_COUNT_RING_1 257
#define LED_COUNT_RING_2 212
#define LED_COUNT_RING_3 179
#define LED_COUNT_TOTAL  648

const int ring_1_midpoint_L = (int)(LED_COUNT_RING_1 * 0.25);
const int ring_1_midpoint_R = (int)(LED_COUNT_RING_1 * 0.75 + 1); // fudge factor
const int ring_2_midpoint_L = (int)(LED_COUNT_RING_2 * 0.25 + LED_COUNT_RING_1);
const int ring_2_midpoint_R = (int)(LED_COUNT_RING_2 * 0.75 + LED_COUNT_RING_1 - 1); // fudge factor
const int ring_3_midpoint_L = (int)(LED_COUNT_RING_3 * 0.25 + LED_COUNT_RING_2 + LED_COUNT_RING_1);
const int ring_3_midpoint_R = (int)(LED_COUNT_RING_3 * 0.75 + LED_COUNT_RING_2 + LED_COUNT_RING_1);

extern unsigned long lastBeatTimestamp;  // Defined in beatdetection.cpp
extern unsigned long elapsedBeats;

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
		} else {
			t_mod = 1.0f - fractional;  // Interpolates from 1 to 0
		}
		return LedColor::interpolate(color1, color2, t_mod);
	}
};

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
	LedColor(&ledColors)[LED_COUNT_TOTAL];
	String name;
	// Helper methods
	void fill(LedColor color, int start, int length) {
		for (int i = start; i < start + length; i++) {
			ledColors[i] = color;
		}
	}
public:
	Shader(LedColor(&colors)[LED_COUNT_TOTAL], String shaderName) : ledColors(colors), name(shaderName) {}
	virtual void update(int frame) = 0;  // Pure virtual function to be implemented by each shader
	String getName() const {
		return name;
	}
};

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
	WhiteOverRainbow(LedColor(&colors)[LED_COUNT_TOTAL]) : Shader(colors, "White Over Rainbow") {}
	void update(int frame) override {
		if (whiteLength >= LED_COUNT_TOTAL) return; // Error handling

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

class Inferno : public Shader {
private:
	int cycleTime = 20;  // Determines how quickly the colors cycle
	int periods = 2;

	// Define hue ranges for each ring. Values are in degrees (0-360) on the color wheel.
	struct HueRange {
		uint16_t startHue;
		uint16_t endHue;
	};

	HueRange ringHueRanges[3] = {
		{240, 270},  // Ring 1: Blue to Purple (240 is blue, 270 is violet)
		{270, 340},  // Ring 2: Purple to Magenta (270 is violet, 300 is magenta)
		{0, 40}    // Ring 3: Magenta to Red to Yellow (300 is magenta, 60 is yellow)
	};

public:
	Inferno(LedColor(&colors)[LED_COUNT_TOTAL]) : Shader(colors, "Inferno") {}

	void update(int frame) override {
		for (int i = 0; i < LED_COUNT_RING_1; i++) {
			ledColors[i] = hueInterpolate(i, ringHueRanges[0], frame, LED_COUNT_RING_1);
		}
		for (int i = 0; i < LED_COUNT_RING_2; i++) {
			ledColors[i + LED_COUNT_RING_1] = hueInterpolate(i, ringHueRanges[1], frame, LED_COUNT_RING_2);
		}
		for (int i = 0; i < LED_COUNT_RING_3; i++) {
			ledColors[i + LED_COUNT_RING_1 + LED_COUNT_RING_2] = hueInterpolate(i, ringHueRanges[2], frame, LED_COUNT_RING_3);
		}
	}

	LedColor hueInterpolate(int ledIndex, HueRange hueRange, int frame, int totalLeds) {
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

class Inferno2 : public Shader {
private:
	int cycleTime = 50;  // Determines how quickly the colors cycle
	int periods = 1;

	LedColor blue;
	LedColor purple;
	LedColor magenta;
	LedColor orange;
	LedColor brightOrange;
	LedColor yellow;
public:
	Inferno2(LedColor(&colors)[LED_COUNT_TOTAL]) : Shader(colors, "Inferno2"),
		blue(15,7,136,0),
		purple(156,24,157,0),
		magenta(230, 22, 98,0),
		orange(228,91,47, 0),
		brightOrange(243,118,25,0),
		yellow(245,233,37,0)
	{};

	void update(int frame) override {
		for (int i = 0; i < LED_COUNT_RING_1; i++) {
			ledColors[i] = LedColor::interpolateZigZag(blue, purple, float(2 * periods * i) / LED_COUNT_RING_1);
			// ledColors[i] = getColorForLed(i, ringHueRanges[0], frame, LED_COUNT_RING_1);
		}
		for (int i = 0; i < LED_COUNT_RING_2; i++) {
			ledColors[i + LED_COUNT_RING_1] = LedColor::interpolateZigZag(magenta, orange, float(2 * periods * i) / LED_COUNT_RING_2);
			// ledColors[i + LED_COUNT_RING_1] = getColorForLed(i, ringHueRanges[1], frame, LED_COUNT_RING_2);
		}
		for (int i = 0; i < LED_COUNT_RING_3; i++) {
			ledColors[i + LED_COUNT_RING_1 + LED_COUNT_RING_2] = LedColor::interpolateZigZag(brightOrange, yellow, float(2 * periods * i) / LED_COUNT_RING_3);
			// ledColors[i + LED_COUNT_RING_1 + LED_COUNT_RING_2] = getColorForLed(i, ringHueRanges[2], frame, LED_COUNT_RING_3);
		}
	}
};

class BluePurple : public Shader {
private:
	int cycleTime = 50;  // Determines how quickly the colors cycle
	int periods = 1;

	LedColor cyan;
	LedColor blue;
	LedColor purple;
	LedColor magenta;
public:
	BluePurple(LedColor(&colors)[LED_COUNT_TOTAL]) : Shader(colors, "BluePurple"),
		cyan(0, 200, 200, 0),
		blue(0, 20, 255, 0),
		purple(156, 0, 255, 0),
		magenta(255, 0, 255, 0)
	{};

	void update(int frame) override {
		for (int i = 0; i < LED_COUNT_RING_1; i++) {
			ledColors[i] = LedColor::interpolateZigZag(cyan, blue, float(2 * periods * i) / LED_COUNT_RING_1);
		}
		for (int i = 0; i < LED_COUNT_RING_2; i++) {
			ledColors[i + LED_COUNT_RING_1] = LedColor::interpolateZigZag(blue, purple, float(2 * periods * i) / LED_COUNT_RING_2);
		}
		for (int i = 0; i < LED_COUNT_RING_3; i++) {
			ledColors[i + LED_COUNT_RING_1 + LED_COUNT_RING_2] = LedColor::interpolateZigZag(purple, magenta, float(2 * periods * i) / LED_COUNT_RING_3);
		}
	}
};

class AquaColors : public Shader {
private:
	int cycleTime = 30;  // Determines how quickly the colors cycle
	int periods = 3;

	// Define hue ranges for each ring. Values are in degrees (0-360) on the color wheel, then converted to 0-65535.
	struct HueRange {
		uint16_t startHue;
		uint16_t endHue;
	};

	HueRange ringHueRanges[3] = {
		{170, 200},  // Ring 1: Blues to Purples (170 is light blue, 270 is violet)
		{190, 220},  // Ring 2: Cyan Colors (180 is cyan, 210 is deeper cyan)
		{150, 180}   // Ring 3: Turquoise Colors (150 is soft turquoise, 180 is cyan)
	};

public:
	AquaColors(LedColor(&colors)[LED_COUNT_TOTAL]) : Shader(colors, "Aqua Colors") {}

	void update(int frame) override {
		for (int i = 0; i < LED_COUNT_RING_1; i++) {
			ledColors[i] = hueInterpolate(i, ringHueRanges[0], frame, LED_COUNT_RING_1);
		}
		for (int i = 0; i < LED_COUNT_RING_2; i++) {
			ledColors[i + LED_COUNT_RING_1] = hueInterpolate(i, ringHueRanges[1], frame, LED_COUNT_RING_2);
		}
		for (int i = 0; i < LED_COUNT_RING_3; i++) {
			ledColors[i + LED_COUNT_RING_1 + LED_COUNT_RING_2] = hueInterpolate(i, ringHueRanges[2], frame, LED_COUNT_RING_3);
		}
	}

	// LedColor getColorForLed(int ledIndex, HueRange hueRange, int frame, int totalLeds) {
	// 	uint16_t hue = map(int((float(frame) / cycleTime + .5 * sin(2 * PI * periods * float(ledIndex) / totalLeds)) * 65536) % 65536,
	// 		0, 65536, hueRange.startHue, hueRange.endHue);
	// 	return LedColor(Adafruit_NeoPixel::gamma32(Adafruit_NeoPixel::ColorHSV(hue * 182)));  // Multiply by 182 to convert 0-360 to 0-65535
	// }

	LedColor hueInterpolate(int ledIndex, HueRange hueRange, int frame, int totalLeds) {
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


class RedSineWave : public Shader {
private:
	int periodsPerRing = 3;
	int p = 2;
	float speed = 0.015;
public:
	RedSineWave(LedColor(&colors)[LED_COUNT_TOTAL]) : Shader(colors, "Red Sine Waves") {}
	void update(int frame) override {
		for (int i = 0; i < LED_COUNT_RING_1; i++) {
			float theta = 2 * PI * (float(i) / LED_COUNT_RING_1 + frame * speed * .7);
			ledColors[i] = sinLoops(LedColor(255, 0, 0, 0), theta * periodsPerRing, p);
		}
		for (int i = 0; i < LED_COUNT_RING_2; i++) {
			float theta = 2 * PI * (float(i) / LED_COUNT_RING_2 + frame * speed * 1.0);
			ledColors[i + LED_COUNT_RING_1] = sinLoops(LedColor(255, 0, 0, 0), theta * periodsPerRing, p);
		}
		for (int i = 0; i < LED_COUNT_RING_3; i++) {
			float theta = 2 * PI * (float(i) / LED_COUNT_RING_3 + frame * speed * 1.5);
			ledColors[i + LED_COUNT_RING_1 + LED_COUNT_RING_2] = sinLoops(LedColor(255, 0, 0, 0), theta * periodsPerRing, p);
		}
	}
};


class RedSquareWave : public Shader {
private:
	int periodsPerRing = 3;
	int p = 4;
	float speed = 0.01;
public:
	RedSquareWave(LedColor(&colors)[LED_COUNT_TOTAL]) : Shader(colors, "Red Square Wave") {}
	void update(int frame) override {
		for (int i = 0; i < LED_COUNT_RING_1; i++) {
			float theta = 2 * PI * (float(i) / LED_COUNT_RING_1 + frame * speed);
			ledColors[i] = squareLoops(LedColor(255, 0, 0, 0), theta * periodsPerRing);
		}
		for (int i = 0; i < LED_COUNT_RING_2; i++) {
			float theta = 2 * PI * (float(i) / LED_COUNT_RING_2 + frame * speed);
			ledColors[i + LED_COUNT_RING_1] = squareLoops(LedColor(255, 15, 0, 0), theta * periodsPerRing);
		}
		for (int i = 0; i < LED_COUNT_RING_3; i++) {
			float theta = 2 * PI * (float(i) / LED_COUNT_RING_3 + frame * speed);
			ledColors[i + LED_COUNT_RING_1 + LED_COUNT_RING_2] = squareLoops(LedColor(255, 30, 0, 0), theta * periodsPerRing);
		}
	}
};




class AccentShader {
protected:
	// Adafruit_NeoPixel& strip;
	LedColor(&ledColors)[LED_COUNT_TOTAL];
	String name;
	// Helper methods
	void fill(LedColor color, int start, int length) {
		for (int i = start; i < start + length; i++) {
			ledColors[i] = color;
		}
	}
public:
	AccentShader(LedColor(&colors)[LED_COUNT_TOTAL], String shaderName) : ledColors(colors), name(shaderName) {}
	virtual void update(int frame, float intensity) = 0;
	String getName() const {
		return name;
	}
};

class NoAccent : public AccentShader {
private:
public:
	NoAccent(LedColor(&colors)[LED_COUNT_TOTAL]) : AccentShader(colors, "(No Accent)") {}
	void update(int frame, float intensity) override {

	}
};

class WhitePeaks : public AccentShader {
private:
	float factor = 0.07;
	float maxWhiteAmount = 0.45;
	float minWhiteCutoff = 0.05;
	bool usePureWhite = true;
	float peak = 0.0;
	float falloff = 1.0 / (30.0 * 0.25); // 0.25 second falloff
public:
	WhitePeaks(LedColor(&colors)[LED_COUNT_TOTAL]) : AccentShader(colors, "White Peaks") {}
	void update(int frame, float intensity) override {
		peak = std::max(std::min(std::min(factor * intensity, float(1)), maxWhiteAmount), peak);
		if (peak < minWhiteCutoff) {
			return;
		}
		int amp1 = peak * LED_COUNT_RING_1 / 4;
		int amp2 = peak * LED_COUNT_RING_2 / 4;
		int amp3 = peak * LED_COUNT_RING_3 / 4;
		// Serial.println(amp1);
		// Serial.println(amp2);
		// Serial.println(amp3);
		if (intensity > 0) {
			if (usePureWhite) {
				int ring_2_start = LED_COUNT_RING_1;
				int ring_2_end = LED_COUNT_RING_1 + LED_COUNT_RING_2;
				int ring_2_middle = (ring_2_start + ring_2_end) / 2;
				fill(LedColor(255, 255, 255, 255), ring_1_midpoint_L - amp1, 2 * amp1);
				fill(LedColor(255, 255, 255, 255), ring_1_midpoint_R - amp1, 2 * amp1);
				fill(LedColor(255, 255, 255, 255), ring_2_start, amp2);
				fill(LedColor(255, 255, 255, 255), ring_2_end - amp2, amp2);
				fill(LedColor(255, 255, 255, 255), ring_2_middle - amp2, 2 * amp2);
				fill(LedColor(255, 255, 255, 255), ring_3_midpoint_L - amp3, 2 * amp3);
				fill(LedColor(255, 255, 255, 255), ring_3_midpoint_R - amp3, 2 * amp3);
			}
			else {
				for (int i = ring_1_midpoint_L - amp1; i < ring_1_midpoint_L + amp1; i++) { ledColors[i].w = 255; }
				for (int i = ring_1_midpoint_R - amp1; i < ring_1_midpoint_R + amp1; i++) { ledColors[i].w = 255; }
				for (int i = ring_2_midpoint_L - amp2; i < ring_2_midpoint_L + amp2; i++) { ledColors[i].w = 255; }
				for (int i = ring_2_midpoint_R - amp2; i < ring_2_midpoint_R + amp2; i++) { ledColors[i].w = 255; }
				for (int i = ring_3_midpoint_L - amp3; i < ring_3_midpoint_L + amp3; i++) { ledColors[i].w = 255; }
				for (int i = ring_3_midpoint_R - amp3; i < ring_3_midpoint_R + amp3; i++) { ledColors[i].w = 255; }
			}
		}
		peak -= falloff;
		// Serial.println("done");
	}
};


class WhitePeaksBeatsOnly : public AccentShader {
private:
	float factor = 0.07;
	float maxWhiteAmount = 0.6;
	float minWhiteCutoff = 0.01;
	bool usePureWhite = true;
	float peak = 0.0;
	float falloff = 1.0 / (30.0 * 0.15); // 0.15 second falloff
	float lastBeatTimestamp_internal = 0;
public:
	WhitePeaksBeatsOnly(LedColor(&colors)[LED_COUNT_TOTAL]) : AccentShader(colors, "White Peaks (Beats Only)") {}
	void update(int frame, float intensity) override {

		if (lastBeatTimestamp_internal != lastBeatTimestamp) {
			// peak = std::max(std::min(std::min(factor * intensity, float(1)), maxWhiteAmount), peak);
			peak = maxWhiteAmount;
			lastBeatTimestamp_internal = lastBeatTimestamp;
		}

		if (peak < minWhiteCutoff) {
			return;
		}
		int amp1 = peak * LED_COUNT_RING_1 / 4;
		int amp2 = peak * LED_COUNT_RING_2 / 4;
		int amp3 = peak * LED_COUNT_RING_3 / 4;
		// Serial.println(amp1);
		// Serial.println(amp2);
		// Serial.println(amp3);
		if (intensity > 0) {
			if (usePureWhite) {
				int ring_2_start = LED_COUNT_RING_1;
				int ring_2_end = LED_COUNT_RING_1 + LED_COUNT_RING_2;
				int ring_2_middle = (ring_2_start + ring_2_end) / 2;
				fill(LedColor(255, 255, 255, 255), ring_1_midpoint_L - amp1, 2 * amp1);
				fill(LedColor(255, 255, 255, 255), ring_1_midpoint_R - amp1, 2 * amp1);
				fill(LedColor(255, 255, 255, 255), ring_2_start, amp2);
				fill(LedColor(255, 255, 255, 255), ring_2_end - amp2, amp2);
				fill(LedColor(255, 255, 255, 255), ring_2_middle - amp2, 2 * amp2);
				fill(LedColor(255, 255, 255, 255), ring_3_midpoint_L - amp3, 2 * amp3);
				fill(LedColor(255, 255, 255, 255), ring_3_midpoint_R - amp3, 2 * amp3);
			}
			else {
				for (int i = ring_1_midpoint_L - amp1; i < ring_1_midpoint_L + amp1; i++) { ledColors[i].w = 255; }
				for (int i = ring_1_midpoint_R - amp1; i < ring_1_midpoint_R + amp1; i++) { ledColors[i].w = 255; }
				for (int i = ring_2_midpoint_L - amp2; i < ring_2_midpoint_L + amp2; i++) { ledColors[i].w = 255; }
				for (int i = ring_2_midpoint_R - amp2; i < ring_2_midpoint_R + amp2; i++) { ledColors[i].w = 255; }
				for (int i = ring_3_midpoint_L - amp3; i < ring_3_midpoint_L + amp3; i++) { ledColors[i].w = 255; }
				for (int i = ring_3_midpoint_R - amp3; i < ring_3_midpoint_R + amp3; i++) { ledColors[i].w = 255; }
			}
		}
		peak -= falloff;
		// Serial.println("done");
	}
};

class LightFanIn : public AccentShader {
private:
	float animationTime = 350.0;
	float whiteFractionOfArc = 0.3;
public:
	LightFanIn(LedColor(&colors)[LED_COUNT_TOTAL]) : AccentShader(colors, "Light Fan-In") {}
	void update(int frame, float intensity) override {
		
		float amp = float(millis() - lastBeatTimestamp) / animationTime;

		if (amp <= 1.0) {
			int amp1 = amp * LED_COUNT_RING_1 / 4;
			int amp2 = amp * LED_COUNT_RING_2 / 4;
			int amp3 = amp * LED_COUNT_RING_3 / 4;

			int white1 = whiteFractionOfArc * LED_COUNT_RING_1 / 4;
			int white2 = whiteFractionOfArc * LED_COUNT_RING_2 / 4;
			int white3 = whiteFractionOfArc * LED_COUNT_RING_3 / 4;

			if (amp + whiteFractionOfArc > 1) {
				white1 *= 1 - (amp + whiteFractionOfArc - 1) / whiteFractionOfArc;
				white2 *= 1 - (amp + whiteFractionOfArc - 1) / whiteFractionOfArc;
				white3 *= 1 - (amp + whiteFractionOfArc - 1) / whiteFractionOfArc;
			}

			int ring_2_start = LED_COUNT_RING_1;
			int ring_2_end = LED_COUNT_RING_1 + LED_COUNT_RING_2;
			int ring_2_middle = (ring_2_start + ring_2_end) / 2;

			fill(LedColor(255, 255, 255, 255), ring_1_midpoint_L + amp1, white1);
			fill(LedColor(255, 255, 255, 255), ring_1_midpoint_L - amp1 - white1, white1);
			fill(LedColor(255, 255, 255, 255), ring_1_midpoint_R + amp1, white1);
			fill(LedColor(255, 255, 255, 255), ring_1_midpoint_R - amp1 - white1, white1);
			
			fill(LedColor(255, 255, 255, 255), ring_2_start + amp2, white2);
			fill(LedColor(255, 255, 255, 255), ring_2_end - amp2 - white2, white2);
			fill(LedColor(255, 255, 255, 255), ring_2_middle + amp2, white2);
			fill(LedColor(255, 255, 255, 255), ring_2_middle - amp2 - white2, white2);

			fill(LedColor(255, 255, 255, 255), ring_3_midpoint_L + amp3, white3);
			fill(LedColor(255, 255, 255, 255), ring_3_midpoint_L - amp3 - white3, white3);
			fill(LedColor(255, 255, 255, 255), ring_3_midpoint_R + amp3, white3);
			fill(LedColor(255, 255, 255, 255), ring_3_midpoint_R - amp3 - white3, white3);
		}
	}
};

class PulsedStrobeOverlay : public AccentShader {
private:
	float brightnessScale = 0.25;
public:
	PulsedStrobeOverlay(LedColor(&colors)[LED_COUNT_TOTAL]) : AccentShader(colors, "Pulsed Strobe Overlay") {}
	void update(int frame, float intensity) override {
		for (int i = 0; i < LED_COUNT_TOTAL; i++) {
			ledColors[i].w = uint8_t(255 * brightnessScale * intensity);
		}
	}
};

class Strobe : public AccentShader {
private:
	float brightnessScale = 0.8;
	int strobeFrames = 15;
public:
	Strobe(LedColor(&colors)[LED_COUNT_TOTAL]) : AccentShader(colors, "Strobe") {}
	void update(int frame, float intensity) override {
		if (frame % strobeFrames < strobeFrames / 2) {
			for (int i = 0; i < LED_COUNT_TOTAL; i++) {
				ledColors[i].w = uint8_t(255 * brightnessScale * intensity);
			}
		}
	}
};

class BeatHueShift : public AccentShader {
private:
	float hueShiftFactor = 0.0002;
	float cutoff = 1.0;
	uint16_t phase = 0;  // Tracks the last hue shift to create a continuous effect
public:
	BeatHueShift(LedColor(&colors)[LED_COUNT_TOTAL]) : AccentShader(colors, "Beat Hue Shift") {}

	void update(int frame, float intensity) override {
		// Calculate the hue shift as a 16-bit value (0 to 65535), proportional to intensity
		if (intensity < cutoff) {
			return;
		}
		uint16_t hueShift = (uint16_t)(intensity * intensity * hueShiftFactor * 65535) % 65535;
		phase = (phase + hueShift) % 65535;

		for (int i = 0; i < LED_COUNT_TOTAL; i++) {
			// Convert original RGB to its approximate HSV values for hue manipulation
			uint16_t hue;
			uint8_t sat, val;
			rgbToApproximateHsv(ledColors[i].r, ledColors[i].g, ledColors[i].b, hue, sat, val);
			// Apply hue shift
			hue = (hue + phase) % 65535;
			// Convert back to RGB using Adafruit's HSV to RGB conversion
			ledColors[i] = LedColor(Adafruit_NeoPixel::ColorHSV(hue, sat, val));
			ledColors[i].r *= constrain(.5 * (1 + intensity), 0., 1.);
			ledColors[i].g *= constrain(.5 * (1 + intensity), 0., 1.);
			ledColors[i].b *= constrain(.5 * (1 + intensity), 0., 1.);
			ledColors[i].w *= constrain(.5 * (1 + intensity), 0., 1.);
		}
	}

	// A simple approximation method to convert RGB to HSV
	// This is a placeholder for you to implement a more accurate conversion if needed
	void rgbToApproximateHsv(uint8_t r, uint8_t g, uint8_t b, uint16_t& hue, uint8_t& sat, uint8_t& val) {
		uint8_t min, max, delta;
		min = r < g ? (r < b ? r : b) : (g < b ? g : b);
		max = r > g ? (r > b ? r : b) : (g > b ? g : b);
		val = max;  // value is the maximum of r, g, b
		delta = max - min;
		if (delta == 0) { // gray, no color
			hue = 0;
			sat = 0;
		}
		else { // Colorful
			sat = 255 * delta / max;
			if (r == max)
				hue = (g - b) / (float)delta; // between yellow & magenta
			else if (g == max)
				hue = 2 + (b - r) / (float)delta; // between cyan & yellow
			else
				hue = 4 + (r - g) / (float)delta; // between magenta & cyan
			hue *= 60; // degrees
			if (hue < 0)
				hue += 360;
			hue = (uint16_t)(hue / 360.0 * 65535);  // Convert to 0-65535 range
		}
	}
};

class BeatHueShift2 : public AccentShader {
private:
	float hueShiftAngle = 50.0;
	float theta = 0.0;
public:
	BeatHueShift2(LedColor(&colors)[LED_COUNT_TOTAL]) : AccentShader(colors, "Beat Hue Shift 2") {}

	void update(int frame, float intensity) override {
		float dTheta = 0.5;
		theta = fmod(theta + dTheta, 360.0);
		float angle = fmod(hueShiftAngle * (elapsedBeats % (360/30)), 360.0) + theta;
		uint16_t phase = uint16_t(fmod(angle, 360.) / 360.0 * 65535) % 65535;

		for (int i = 0; i < LED_COUNT_TOTAL; i++) {
			// Convert original RGB to its approximate HSV values for hue manipulation
			uint16_t hue;
			uint8_t sat, val;
			rgbToApproximateHsv(ledColors[i].r, ledColors[i].g, ledColors[i].b, hue, sat, val);
			// Apply hue shift
			hue = (hue + phase) % 65535;
			// Convert back to RGB using Adafruit's HSV to RGB conversion
			ledColors[i] = LedColor(Adafruit_NeoPixel::ColorHSV(hue, sat, val));
			ledColors[i].r *= constrain(.5 * (1 + intensity), 0., 1.);
			ledColors[i].g *= constrain(.5 * (1 + intensity), 0., 1.);
			ledColors[i].b *= constrain(.5 * (1 + intensity), 0., 1.);
			ledColors[i].w *= constrain(.5 * (1 + intensity), 0., 1.);
		}
	}

	// A simple approximation method to convert RGB to HSV
	// This is a placeholder for you to implement a more accurate conversion if needed
	void rgbToApproximateHsv(uint8_t r, uint8_t g, uint8_t b, uint16_t& hue, uint8_t& sat, uint8_t& val) {
		uint8_t min, max, delta;
		min = r < g ? (r < b ? r : b) : (g < b ? g : b);
		max = r > g ? (r > b ? r : b) : (g > b ? g : b);
		val = max;  // value is the maximum of r, g, b
		delta = max - min;
		if (delta == 0) { // gray, no color
			hue = 0;
			sat = 0;
		}
		else { // Colorful
			sat = 255 * delta / max;
			if (r == max)
				hue = (g - b) / (float)delta; // between yellow & magenta
			else if (g == max)
				hue = 2 + (b - r) / (float)delta; // between cyan & yellow
			else
				hue = 4 + (r - g) / (float)delta; // between magenta & cyan
			hue *= 60; // degrees
			if (hue < 0)
				hue += 360;
			hue = (uint16_t)(hue / 360.0 * 65535);  // Convert to 0-65535 range
		}
	}
};



class ShaderManager {
private:
	Adafruit_NeoPixel& strip;
	LedColor(&ledColors)[LED_COUNT_TOTAL];
public:
	std::map<String, Shader*> shaders;
	std::map<String, AccentShader*> accentShaders;
	Shader* activeShader = nullptr;
	AccentShader* activeAccentShader = nullptr;
	ShaderManager(Adafruit_NeoPixel& ledStrip, LedColor(&colors)[LED_COUNT_TOTAL]) : strip(ledStrip), ledColors(colors) {
		std::vector<Shader*> shaderList = {
			new LoopyRainbow(ledColors),
			new LoopyRainbow2(ledColors),
			new WhiteOverRainbow(ledColors),
			new Inferno(ledColors),
			new Inferno2(ledColors),
			new RedSineWave(ledColors),
			new RedSquareWave(ledColors),
			new AquaColors(ledColors),
			new BluePurple(ledColors)
			// Add more shaders here
		};

		std::vector<AccentShader*> accentShaderList = {
			new NoAccent(ledColors),
			new WhitePeaks(ledColors),
			new WhitePeaksBeatsOnly(ledColors),
			new PulsedStrobeOverlay(ledColors),
			new Strobe(ledColors),
			new BeatHueShift(ledColors),
			new BeatHueShift2(ledColors),
			new LightFanIn(ledColors)
			// Add more shaders here
		};

		for (Shader* shader : shaderList) {
			shaders[shader->getName()] = shader;
		}

		for (AccentShader* shader : accentShaderList) {
			accentShaders[shader->getName()] = shader;
		}

		if (!shaderList.empty()) {
			activeShader = shaderList[0]; // Set the first shader as the default active shader
		}

		if (!accentShaderList.empty()) {
			activeAccentShader = accentShaderList[0]; // Set the first shader as the default active shader
		}
	}

	~ShaderManager() {
		for (auto& shader : shaders) {
			delete shader.second;
		}
		for (auto& shader : accentShaders) {
			delete shader.second;
		}
	}

	void setActiveShader(const String& shaderName) {
		auto it = shaders.find(shaderName);
		if (it != shaders.end()) {
			activeShader = it->second;
		}
		else {
			Serial.println("Shader not found");
		}
	}

	void setActiveAccentShader(const String& shaderName) {
		auto it = accentShaders.find(shaderName);
		if (it != accentShaders.end()) {
			activeAccentShader = it->second;
		}
		else {
			Serial.println("Accent Shader not found");
		}
	}

	void run(int frame, float intensity) {
		activeShader->update(frame);
		activeAccentShader->update(frame, intensity);
		// Flush the ledColors to the strip
		for (int i = 0; i < LED_COUNT_TOTAL; i++) {
			strip.setPixelColor(i, ledColors[i].pack());
		}
		strip.show();
	}

};
