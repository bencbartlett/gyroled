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

class Shader {
protected:
	Adafruit_NeoPixel& strip;
	String name;
public:
	Shader(Adafruit_NeoPixel& ledStrip, String shaderName) : strip(ledStrip), name(shaderName) {}
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
	WhiteOverRainbow(Adafruit_NeoPixel& ledStrip) : Shader(ledStrip, "White Over Rainbow") {}

	void update(int frame) override {
		if (whiteLength >= strip.numPixels()) return; // Error handling

		for (int i = 0; i < strip.numPixels(); i++) {
			if (((i >= tail) && (i <= head)) || ((tail > head) && ((i >= tail) || (i <= head)))) {
				strip.setPixelColor(i, strip.Color(255, 255, 255, 255)); // Set white
			}
			else {
				int pixelHue = firstPixelHue + (i * 65536L / strip.numPixels());
				strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(pixelHue)));
			}
		}

		firstPixelHue += 40;

		if ((millis() - lastTime) > whiteSpeed) {
			if (++head >= strip.numPixels()) {
				head = 0;
				if (++loopNum >= loops) loopNum = 0;
			}
			if (++tail >= strip.numPixels()) {
				tail = 0;
			}
			lastTime = millis();
		}
	}
};

class LoopyRainbow : public Shader {
private:
	int fadeVal = 100;
	int fadeMax = 100;

public:
	LoopyRainbow(Adafruit_NeoPixel& ledStrip) : Shader(ledStrip, "Loopy Rainbow") {}

	void update(int frame) override {

		for (int i = 0; i < strip.numPixels(); i++) {
			uint32_t pixelHue = frame * 1000 + (i * 65536L / strip.numPixels());
			strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(pixelHue, 255, 255 * fadeVal / fadeMax)));
		}

		frame++;
	}
};

class AccentShader {
protected:
	Adafruit_NeoPixel& strip;
	String name;
public:
	AccentShader(Adafruit_NeoPixel& ledStrip, String shaderName) : strip(ledStrip), name(shaderName) {}
	virtual void update(int frame, float intensity) = 0;  // Pure virtual function to be implemented by each shader
	String getName() const {
		return name;
	}
};


class WhitePeaks : public AccentShader {
private:
	float factor = 0.8;
public:
	WhitePeaks(Adafruit_NeoPixel& ledStrip) : AccentShader(ledStrip, "White Peaks") {}

	void update(int frame, float intensity) override {
		int amp1 = factor * intensity * LED_COUNT_RING_1 / 4;
		int amp2 = factor * intensity * LED_COUNT_RING_2 / 4;
		int amp3 = factor * intensity * LED_COUNT_RING_3 / 4;
		if (intensity > 0) {
			strip.fill(strip.Color(255, 255, 255, 255), ring_1_midpoint_L - amp1, 2 * amp1);
			strip.fill(strip.Color(255, 255, 255, 255), ring_1_midpoint_R - amp1, 2 * amp1);
			strip.fill(strip.Color(255, 255, 255, 255), ring_2_midpoint_L - amp2, 2 * amp2);
			strip.fill(strip.Color(255, 255, 255, 255), ring_2_midpoint_R - amp2, 2 * amp2);
			strip.fill(strip.Color(255, 255, 255, 255), ring_3_midpoint_L - amp3, 2 * amp3);
			strip.fill(strip.Color(255, 255, 255, 255), ring_3_midpoint_R - amp3, 2 * amp3);
		}
	}
};

class ShaderManager {
private:
    Adafruit_NeoPixel& strip;
public:
	std::map<String, Shader*> shaders;
	std::map<String, AccentShader*> accentShaders;
	Shader* activeShader = nullptr;
	AccentShader* activeAccentShader = nullptr;
    ShaderManager(Adafruit_NeoPixel& ledStrip) : strip(ledStrip) {
		std::vector<Shader*> shaderList = {
            new WhiteOverRainbow(strip),
            new LoopyRainbow(strip)
            // Add more shaders here
        };

		std::vector<AccentShader*> accentShaderList = {
            new WhitePeaks(strip)
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
        } else {
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

};
