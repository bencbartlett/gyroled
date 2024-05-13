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
	LedColor(uint8_t red = 0, uint8_t green = 0, uint8_t blue = 0, uint8_t white = 0) : r(red), g(green), b(blue), w(white) {}
	uint32_t pack() const {
		return Adafruit_NeoPixel::Color(r, g, b, w);
	}
};



class Shader {
protected:
	Adafruit_NeoPixel& strip;
	LedColor(&ledColors)[LED_COUNT_TOTAL];
	String name;
public:
	Shader(Adafruit_NeoPixel& ledStrip, LedColor(&colors)[LED_COUNT_TOTAL]) : strip(ledStrip), ledColors(colors) {}
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
protected:
	String name = "Loopy Rainbow";
private:
	int fadeVal = 100;
	int fadeMax = 100;
public:
	using Shader::Shader;
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
	LedColor(&ledColors)[LED_COUNT_TOTAL];
	String name;
public:
	AccentShader(Adafruit_NeoPixel& ledStrip, LedColor(&colors)[LED_COUNT_TOTAL]) : strip(ledStrip), ledColors(colors) {}
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
	LedColor(&ledColors)[LED_COUNT_TOTAL];
public:
	std::map<String, Shader*> shaders;
	std::map<String, AccentShader*> accentShaders;
	Shader* activeShader = nullptr;
	AccentShader* activeAccentShader = nullptr;
	ShaderManager(Adafruit_NeoPixel& ledStrip, LedColor(&colors)[LED_COUNT_TOTAL]) : strip(ledStrip), ledColors(colors) {
		std::vector<Shader*> shaderList = {
			new WhiteOverRainbow(strip, ledColors),
			new LoopyRainbow(strip, ledColors)
			// Add more shaders here
		};

		std::vector<AccentShader*> accentShaderList = {
			new NoAccent(strip, ledColors),
			new WhitePeaks(strip, ledColors)
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
	}

};
