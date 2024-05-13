#include <map>
#include <vector>
#include <Adafruit_NeoPixel.h>

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

struct LedColor {
	uint8_t r, g, b, w;
	LedColor(uint32_t color) {
		w = (color >> 24) & 0xFF;
		r = (color >> 16) & 0xFF;
		g = (color >> 8) & 0xFF;
		b = color & 0xFF;
	}
	LedColor(uint8_t red = 0, uint8_t green = 0, uint8_t blue = 0, uint8_t white = 0) : r(red), g(green), b(blue), w(white) {}
	uint32_t pack() const {
		return Adafruit_NeoPixel::Color(r, g, b, w);
	}
};



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
	Shader(LedColor(&colors)[LED_COUNT_TOTAL]) : ledColors(colors) {}
	virtual void update(int frame) = 0;  // Pure virtual function to be implemented by each shader
	String getName() const {
		return name;
	}
};

class WhiteOverRainbow : public Shader {
protected:
	String name = "White Over Rainbow";
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
	using Shader::Shader;
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
protected:
	String name = "Loopy Rainbow";
private:
	int cycleSpeed = 5000;
	int fadeVal = 100;
	int fadeMax = 100;
public:
	using Shader::Shader;
	void update(int frame) override {
		for (int i = 0; i < LED_COUNT_TOTAL; i++) {
			uint32_t pixelHue = frame * cycleSpeed + (i * 65536L / LED_COUNT_TOTAL);
			ledColors[i] = LedColor(Adafruit_NeoPixel::gamma32(Adafruit_NeoPixel::ColorHSV(pixelHue, 255, 255 * fadeVal / fadeMax)));
		}
		frame++;
	}
};

class Inferno : public Shader {
protected:
    String name = "Inferno";
private:
    int cycleSpeed = 5000;  // Determines how quickly the colors cycle

    // Define hue ranges for each ring. Values are in degrees (0-360) on the color wheel.
    struct HueRange {
        uint16_t startHue;
        uint16_t endHue;
    };

    HueRange ringHueRanges[3] = {
        {240, 270},  // Ring 1: Blue to Purple (240 is blue, 270 is violet)
        {270, 300},  // Ring 2: Purple to Magenta (270 is violet, 300 is magenta)
        {300, 60}    // Ring 3: Magenta to Red to Yellow (300 is magenta, 60 is yellow)
    };

public:
    using Shader::Shader;

    void update(int frame) override {
        for (int i = 0; i < LED_COUNT_RING_1; i++) {
            ledColors[i] = getColorForLed(i, ringHueRanges[0], frame, LED_COUNT_RING_1);
        }
        for (int i = 0; i < LED_COUNT_RING_2; i++) {
            ledColors[i + LED_COUNT_RING_1] = getColorForLed(i, ringHueRanges[1], frame, LED_COUNT_RING_2);
        }
        for (int i = 0; i < LED_COUNT_RING_3; i++) {
            ledColors[i + LED_COUNT_RING_1 + LED_COUNT_RING_2] = getColorForLed(i, ringHueRanges[2], frame, LED_COUNT_RING_3);
        }
    }

    LedColor getColorForLed(int ledIndex, HueRange hueRange, int frame, int totalLeds) {
        uint16_t hue = map((frame * cycleSpeed / totalLeds + ledIndex) % 65536, 0, 65536, hueRange.startHue, hueRange.endHue);
        return LedColor(Adafruit_NeoPixel::gamma32(Adafruit_NeoPixel::ColorHSV(hue * 182)));  // Multiply by 182 to convert 0-360 to 0-65535
    }
};

class AquaColors : public Shader {
protected:
    String name = "Aqua";
private:
    int cycleSpeed = 5000;  // Determines how quickly the colors cycle

    // Define hue ranges for each ring. Values are in degrees (0-360) on the color wheel, then converted to 0-65535.
    struct HueRange {
        uint16_t startHue;
        uint16_t endHue;
    };

    HueRange ringHueRanges[3] = {
        {170, 270},  // Ring 1: Blues to Purples (170 is light blue, 270 is violet)
        {180, 210},  // Ring 2: Cyan Colors (180 is cyan, 210 is deeper cyan)
        {150, 180}   // Ring 3: Turquoise Colors (150 is soft turquoise, 180 is cyan)
    };

public:
    using Shader::Shader;

    void update(int frame) override {
        for (int i = 0; i < LED_COUNT_RING_1; i++) {
            ledColors[i] = getColorForLed(i, ringHueRanges[0], frame, LED_COUNT_RING_1);
        }
        for (int i = 0; i < LED_COUNT_RING_2; i++) {
            ledColors[i + LED_COUNT_RING_1] = getColorForLed(i, ringHueRanges[1], frame, LED_COUNT_RING_2);
        }
        for (int i = 0; i < LED_COUNT_RING_3; i++) {
            ledColors[i + LED_COUNT_RING_1 + LED_COUNT_RING_2] = getColorForLed(i, ringHueRanges[2], frame, LED_COUNT_RING_3);
        }
    }

    LedColor getColorForLed(int ledIndex, HueRange hueRange, int frame, int totalLeds) {
        uint16_t hue = map((frame * cycleSpeed / totalLeds + ledIndex) % 65536, 0, 65536, hueRange.startHue, hueRange.endHue);
        return LedColor(Adafruit_NeoPixel::gamma32(Adafruit_NeoPixel::ColorHSV(hue * 182)));  // Multiply by 182 to convert 0-360 to 0-65535
    }
};



class RedSineWave : public Shader {
protected:
	String name = "Red Sine Wave";
private:
	int periodsPerRing = 3;
	int moveSpeed = 100;
public:
	using Shader::Shader;
	void update(int frame) override {
		for (int i = 0; i < LED_COUNT_RING_1; i++) {
			float sineVal = sin(2 * PI * i / LED_COUNT_RING_1 * periodsPerRing + frame / moveSpeed);
			ledColors[i] = LedColor(255 * (1 + sineVal) / 2, 0, 0, 0);
		}
		for (int i = 0; i < LED_COUNT_RING_2; i++) {
			float sineVal = sin(2 * PI * i / LED_COUNT_RING_2 * periodsPerRing + frame / moveSpeed);
			ledColors[i + LED_COUNT_RING_1] = LedColor(255 * (1 + sineVal) / 2, 0, 0, 0);
		}
		for (int i = 0; i < LED_COUNT_RING_3; i++) {
			float sineVal = sin(2 * PI * i / LED_COUNT_RING_3 * periodsPerRing + frame / moveSpeed);
			ledColors[i + LED_COUNT_RING_1 + LED_COUNT_RING_2] = LedColor(255 * (1 + sineVal) / 2, 0, 0, 0);
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
	AccentShader(LedColor(&colors)[LED_COUNT_TOTAL]) : ledColors(colors) {}
	virtual void update(int frame, float intensity) = 0;
	String getName() const {
		return name;
	}
};

class NoAccent : public AccentShader {
protected:
	String name = "None";
private:
public:
	using AccentShader::AccentShader;
	void update(int frame, float intensity) override {
		
	}
};


class WhitePeaks : public AccentShader {
protected:
	String name = "White Peaks";
private:
	float factor = 0.8;
public:
	using AccentShader::AccentShader;
	void update(int frame, float intensity) override {
		int amp1 = factor * intensity * LED_COUNT_RING_1 / 4;
		int amp2 = factor * intensity * LED_COUNT_RING_2 / 4;
		int amp3 = factor * intensity * LED_COUNT_RING_3 / 4;
		if (intensity > 0) {
			fill(LedColor(255, 255, 255, 255), ring_1_midpoint_L - amp1, 2 * amp1);
			fill(LedColor(255, 255, 255, 255), ring_1_midpoint_R - amp1, 2 * amp1);
			fill(LedColor(255, 255, 255, 255), ring_2_midpoint_L - amp2, 2 * amp2);
			fill(LedColor(255, 255, 255, 255), ring_2_midpoint_R - amp2, 2 * amp2);
			fill(LedColor(255, 255, 255, 255), ring_3_midpoint_L - amp3, 2 * amp3);
			fill(LedColor(255, 255, 255, 255), ring_3_midpoint_R - amp3, 2 * amp3);
		}
	}
};

class PulsedStrobeOverlay : public AccentShader {
protected:
	String name = "Pulsed Strobe Overlay";
private:
	float brightnessScale = 0.25;
public:
	using AccentShader::AccentShader;
	void update(int frame, float intensity) override {
		for (int i = 0; i < LED_COUNT_TOTAL; i++) {
			ledColors[i].w = uint8_t(255 * brightnessScale * intensity);
		}
	}
};

class Strobe : public AccentShader {
protected:
	String name = "Strobe";
private:
	float brightnessScale = 0.8;
	int strobeFrames = 15;
public:
	using AccentShader::AccentShader;
	void update(int frame, float intensity) override {
		if (frame % strobeFrames < strobeFrames / 2) {
			for (int i = 0; i < LED_COUNT_TOTAL; i++) {
				ledColors[i].w = uint8_t(255 * brightnessScale * intensity);
			}
		}
	}
};

class BeatHueShift : public AccentShader {
protected:
    String name = "Beat Hue Shift";
private:
    uint16_t lastHueShift = 0;  // Tracks the last hue shift to create a continuous effect

public:
    using AccentShader::AccentShader;

    void update(int frame, float intensity) override {
        // Calculate the hue shift as a 16-bit value (0 to 65535), proportional to intensity
        uint16_t hueShift = (uint16_t)(intensity * 65535) % 65535;

        for (int i = 0; i < LED_COUNT_TOTAL; i++) {
            // Convert original RGB to its approximate HSV values for hue manipulation
            uint16_t hue;
            uint8_t sat, val;
            rgbToApproximateHsv(ledColors[i].r, ledColors[i].g, ledColors[i].b, hue, sat, val);
            // Apply hue shift
            hue = (hue + hueShift + lastHueShift) % 65535;
            // Convert back to RGB using Adafruit's HSV to RGB conversion
            ledColors[i] = LedColor(Adafruit_NeoPixel::ColorHSV(hue, sat, val));
        }
        // Update lastHueShift to ensure continuity in the next frame
        lastHueShift = (lastHueShift + hueShift) % 65535;
    }

    // A simple approximation method to convert RGB to HSV
    // This is a placeholder for you to implement a more accurate conversion if needed
    void rgbToApproximateHsv(uint8_t r, uint8_t g, uint8_t b, uint16_t &hue, uint8_t &sat, uint8_t &val) {
        uint8_t min, max, delta;
        min = r < g ? (r < b ? r : b) : (g < b ? g : b);
        max = r > g ? (r > b ? r : b) : (g > b ? g : b);
        val = max;  // value is the maximum of r, g, b
        delta = max - min;
        if (delta == 0) { // gray, no color
            hue = 0;
            sat = 0;
        } else { // Colorful
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
			new WhiteOverRainbow(ledColors),
			new LoopyRainbow(ledColors),
			new Inferno(ledColors),
			new RedSineWave(ledColors),
			new AquaColors(ledColors)
			// Add more shaders here
		};

		std::vector<AccentShader*> accentShaderList = {
			new NoAccent(ledColors),
			new WhitePeaks(ledColors),
			new PulsedStrobeOverlay(ledColors),
			new Strobe(ledColors),
			new BeatHueShift(ledColors)
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
