// #ifndef SHADERS_H
// #define SHADERS_H

// #include <Adafruit_NeoPixel.h>
// #include <map>

// class Shader {
// protected:
//     Adafruit_NeoPixel& strip;
//     String name;

// public:
//     Shader(Adafruit_NeoPixel& ledStrip, String shaderName);
//     virtual ~Shader() = default;
//     virtual void update(int frame) = 0;
//     String getName() const;
// };

// class ShaderManager {
// private:
//     Adafruit_NeoPixel& strip;
// public:
// 	std::map<String, Shader*> shaders;
// 	Shader* activeShader = nullptr;
//     ShaderManager(Adafruit_NeoPixel& ledStrip);
//     ~ShaderManager();
//     void setActiveShader(const String& shaderName);
// };

// // extern Adafruit_NeoPixel strip;
// // extern ShaderManager* shaderManager;

// // void setupLEDs();

// #endif
