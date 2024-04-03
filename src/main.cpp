#include <Arduino.h>
#include <arduinoFFT.h>
// #include <FastLED.h>
#include <Adafruit_NeoPixel.h>
// #include <Servo.h>
#include <ESP32Servo.h>

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

#define BRIGHTNESS 100


#define SAMPLES         64           // CAN"T CHANGE AT THE MOMENT, Must be a power of 2
#define SAMPLING_FREQ   20000         // Hz, must be 40000 or less due to ADC conversion time. Determines maximum frequency that can be analysed by the FFT Fmax=sampleF/2.
#define AMPLITUDE       1000          // Depending on your audio source level, you may need to alter this value. Can be used as a 'sensitivity' control.
#define NUM_BANDS       8             // To change this, you will need to change the bunch of if statements describing the mapping from bins to bands
#define NOISE           20           // Used as a crude noise filter, values below this are ignored
// #define TOP             16
#define USE_AVERAGING   false
#define USE_ROLLING_AMPLITUDE_AVERAGING false
#define PEAK_DECAY_RATE 1
#define NOISE_EMA_ALPHA 0.02        // At about 25fps this is about a 2 second window

float avgNoise = 10.0;

float servo_master_speed = 0.3; // Can range from 0 to 1
float servo1_speed = 0.7; // Can range from -1 to 1
float servo2_speed = 0.8;
float servo3_speed = 1.0;

int ring_1_midpoint_L = (int) (LED_COUNT_RING_1 * 0.25);
int ring_1_midpoint_R = (int) (LED_COUNT_RING_1 * 0.75 + 1); // fudge factor
int ring_2_midpoint_L = (int) (LED_COUNT_RING_2 * 0.25 + LED_COUNT_RING_1);
int ring_2_midpoint_R = (int) (LED_COUNT_RING_2 * 0.75 + LED_COUNT_RING_1 - 1); // fudge factor
int ring_3_midpoint_L = (int) (LED_COUNT_RING_3 * 0.25 + LED_COUNT_RING_2 + LED_COUNT_RING_1);
int ring_3_midpoint_R = (int) (LED_COUNT_RING_3 * 0.75 + LED_COUNT_RING_2 + LED_COUNT_RING_1);

// Sampling and FFT stuff
unsigned int sampling_period_us;
int bandLimits[8] = {3, 6, 13, 27, 55, 112, 229};
double scaleFactor = (double)(SAMPLES/2) / 256.0; // Calculate the scale factor based on new SAMPLES

int peak[SAMPLES];              // The length of these arrays must be >= NUM_BANDS
int oldBarHeights[SAMPLES];
int bandValues[SAMPLES];
float vReal[SAMPLES];
float vImag[SAMPLES];
int noiseLevel = 0;
int frame = 0;
unsigned long newTime;

float updatesPerSecond = 0.0;

// arduinoFFT FFT = arduinoFFT(vReal, vImag, SAMPLES, SAMPLING_FREQ);
ArduinoFFT<float> FFT = ArduinoFFT<float>(vReal, vImag, SAMPLES, SAMPLING_FREQ); /* Create FFT object */





Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_RGBW + NEO_KHZ800);

Servo servo1;
Servo servo2;
Servo servo3;

int head = 10;
int tail = -1; // Start outside of any valid range
int loops = 3;
int loopNum = 0;
uint32_t lastTime = 0;
uint32_t firstPixelHue = 0;
const int whiteSpeed = 5; // Speed of white light moving
const int whiteLength = 30; // Length of the white light
bool updateServo = true; // Flag to control servo update
unsigned long lastServoUpdate = 0;
unsigned long lastPeakUpdate = 0;
unsigned long lastSample = 0;
unsigned long lastUpdate = 0;


void setup() {

  Serial.begin(9600);

  strip.begin();
  strip.clear();
  strip.show();
  strip.setBrightness(BRIGHTNESS);

  ESP32PWM::allocateTimer(0);
	ESP32PWM::allocateTimer(1);
	ESP32PWM::allocateTimer(2);
	ESP32PWM::allocateTimer(3);
	servo1.setPeriodHertz(50);    // standard 50 hz servo
  servo2.setPeriodHertz(50);
  servo3.setPeriodHertz(50);

  servo1.attach(SERVO_1_PIN, 500, 2400);
  servo2.attach(SERVO_2_PIN, 500, 2400);
  servo3.attach(SERVO_3_PIN, 500, 2400);


  sampling_period_us = round(1000000 * (1.0 / SAMPLING_FREQ));
  
  // Reset bandValues[]
  for (int i = 0; i<NUM_BANDS; i++){
    bandValues[i] = 0;
    peak[i] = 0;
    oldBarHeights[i] = 0;
  }

  for (int i = 0; i < 8; i++) {
    bandLimits[i] = (int)(bandLimits[i] * double(SAMPLES/2) / 256.0);
  }
}


/* =====================================================================================
 * COLOR PATTERNS WHEEEEE
 * =====================================================================================
 */

void whiteOverRainbow() {
  if(whiteLength >= strip.numPixels()) return; // Error handling

  for(int i = 0; i < strip.numPixels(); i++) {
    if(((i >= tail) && (i <= head)) || ((tail > head) && ((i >= tail) || (i <= head)))) {
      strip.setPixelColor(i, strip.Color(255, 255, 255, 255)); // Set white
    } else {
      int pixelHue = firstPixelHue + (i * 65536L / strip.numPixels());
      strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(pixelHue)));
    }
  }

  // strip.show();
  firstPixelHue += 40;

  if((millis() - lastTime) > whiteSpeed) {
    if(++head >= strip.numPixels()) {
      head = 0;
      if(++loopNum >= loops) loopNum = 0; // Reset loopNum instead of return
    }
    if(++tail >= strip.numPixels()) {
      tail = 0;
    }
    lastTime = millis();
  }
}

int clip255(int value) {
    // Ensure the value is not less than minVal and not greater than maxVal
    return std::min(std::max(value, 0), 255);
}

void loopyRainbow() {
  int fadeVal=0, fadeMax=100;
  fadeVal = 100;

    for(int i=0; i<strip.numPixels(); i++) { // For each pixel in strip...

      // Offset pixel hue by an amount to make one full revolution of the
      // color wheel (range of 65536) along the length of the strip
      // (strip.numPixels() steps):
      uint32_t pixelHue = frame * 1000 + (i * 65536L / strip.numPixels());

      // strip.ColorHSV() can take 1 or 3 arguments: a hue (0 to 65535) or
      // optionally add saturation and value (brightness) (each 0 to 255).
      // Here we're using just the three-argument variant, though the
      // second value (saturation) is a constant 255.
      uint32_t rgb = strip.gamma32(strip.ColorHSV(pixelHue, 255, 255 * fadeVal / fadeMax));
      // Extract RGB values from the RGB color
      // uint8_t r = (uint8_t)(rgb >> 16), g = (uint8_t)(rgb >> 8), b = (uint8_t)rgb;
      // uint8_t w = clip255(noiseLevel * 3);
      // strip.setPixelColor(i, r, g, b, w);
      strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(pixelHue, 255, 255 * fadeVal / fadeMax)));
    }
}

void applyWhiteBars(int noiseLevel) {
  int factor = 0.95;
  int amp = noiseLevel - avgNoise * factor > 0 ? (noiseLevel - avgNoise * factor) * 1 : 0;
  if (amp > 0)  {
    strip.fill(strip.Color(255, 255, 255, 255), ring_1_midpoint_L - amp, 2 * amp);
    strip.fill(strip.Color(255, 255, 255, 255), ring_1_midpoint_R - amp, 2 * amp);
    strip.fill(strip.Color(255, 255, 255, 255), ring_2_midpoint_L - amp, 2 * amp);
    strip.fill(strip.Color(255, 255, 255, 255), ring_2_midpoint_R - amp, 2 * amp);
    strip.fill(strip.Color(255, 255, 255, 255), ring_3_midpoint_L - amp, 2 * amp);
    strip.fill(strip.Color(255, 255, 255, 255), ring_3_midpoint_R - amp, 2 * amp);
  }
  

  // for(int i=0; i<noiseLevel; i++) { 
  //   strip.setPixelColor(i, strip.Color(255, 255, 255, 255)); 
  // }
}


void pulseWhiteAndRGB() {
  int wait = 5;
  for(int j=0; j<256; j++) { // Ramp up from 0 to 255
    // Fill entire strip with white at gamma-corrected brightness level 'j':
    strip.fill(strip.Color(strip.gamma8(j), strip.gamma8(j), strip.gamma8(j), strip.gamma8(j)));
    strip.show();
    delay(wait);
  }

  for(int j=255; j>=0; j--) { // Ramp down from 255 to 0
    strip.fill(strip.Color(strip.gamma8(j), strip.gamma8(j), strip.gamma8(j), strip.gamma8(j)));
    strip.show();
    delay(wait);
  }
}


void colorWipe(uint32_t color, int wait) {
  for(int i=0; i<strip.numPixels(); i++) { // For each pixel in strip...
    strip.setPixelColor(i, color);         //  Set pixel's color (in RAM)
    strip.show();                          //  Update strip to match
    delay(wait);                           //  Pause for a moment
  }
}



void loop() {

  frame++;

  updatesPerSecond = 1000.0 / float(millis() - lastUpdate);
  lastUpdate = millis();

  String output = "";
  // output += "Updates per second: ";
  output += updatesPerSecond;
  output += "\n";

  // if (millis() - lastSample > 10) {
  #if true
    lastSample = millis();
    // Reset bandValues[]
    for (int i = 0; i<NUM_BANDS; i++){
      bandValues[i] = 0;
    }

    // Sample the audio pin
    for (int i = 0; i < SAMPLES; i++) {
      newTime = micros();
      vReal[i] = analogRead(AUDIO_IN_PIN); // A conversion takes about 9.7uS on an ESP32
      vImag[i] = 0;
      while ((micros() - newTime) < sampling_period_us) { /* chill */ }
    }

    // FFT.dcRemoval();
    FFT.windowing(FFTWindow::Hamming, FFTDirection::Forward);	/* Weigh data */
    FFT.compute(FFTDirection::Forward); /* Compute FFT */
    FFT.complexToMagnitude(); /* Compute magnitudes */
    // float x = FFT.majorPeak();

    // Analyse FFT results
    for (int i = 2; i < (SAMPLES/2); i++){       // Don't use sample 0 and only first SAMPLES/2 are usable. Each array element represents a frequency bin and its value the amplitude.
      if (vReal[i] > NOISE) {                    // Add a crude noise filter

        // // 8 bands, 12kHz top band
        // if (i<=3 )                bandValues[0]  += (int)vReal[i];
        // else if (i>3   && i<=6  ) bandValues[1]  += (int)vReal[i];
        // else if (i>6   && i<=13 ) bandValues[2]  += (int)vReal[i];
        // else if (i>13  && i<=27 ) bandValues[3]  += (int)vReal[i];
        // else if (i>27  && i<=55 ) bandValues[4]  += (int)vReal[i];
        // else if (i>55  && i<=112) bandValues[5]  += (int)vReal[i];
        // else if (i>112 && i<=229) bandValues[6]  += (int)vReal[i];
        // else if (i>229          ) bandValues[7]  += (int)vReal[i];

        if (i<=bandLimits[0])                         bandValues[0]  += (int)vReal[i];
        else if (i>bandLimits[0] && i<=bandLimits[1]) bandValues[1]  += (int)vReal[i];
        else if (i>bandLimits[1] && i<=bandLimits[2]) bandValues[2]  += (int)vReal[i];
        else if (i>bandLimits[2] && i<=bandLimits[3]) bandValues[3]  += (int)vReal[i];
        else if (i>bandLimits[3] && i<=bandLimits[4]) bandValues[4]  += (int)vReal[i];
        else if (i>bandLimits[4] && i<=bandLimits[5]) bandValues[5]  += (int)vReal[i];
        else if (i>bandLimits[5] && i<=bandLimits[6]) bandValues[6]  += (int)vReal[i];
        else if (i>bandLimits[6])                     bandValues[7]  += (int)vReal[i];

        // //16 bands, 12kHz top band
        //   if (i<=2 )           bandValues[0]  += (int)vReal[i];
        //   if (i>2   && i<=3  ) bandValues[1]  += (int)vReal[i];
        //   if (i>3   && i<=5  ) bandValues[2]  += (int)vReal[i];
        //   if (i>5   && i<=7  ) bandValues[3]  += (int)vReal[i];
        //   if (i>7   && i<=9  ) bandValues[4]  += (int)vReal[i];
        //   if (i>9   && i<=13 ) bandValues[5]  += (int)vReal[i];
        //   if (i>13  && i<=18 ) bandValues[6]  += (int)vReal[i];
        //   if (i>18  && i<=25 ) bandValues[7]  += (int)vReal[i];
        //   if (i>25  && i<=36 ) bandValues[8]  += (int)vReal[i];
        //   if (i>36  && i<=50 ) bandValues[9]  += (int)vReal[i];
        //   if (i>50  && i<=69 ) bandValues[10] += (int)vReal[i];
        //   if (i>69  && i<=97 ) bandValues[11] += (int)vReal[i];
        //   if (i>97  && i<=135) bandValues[12] += (int)vReal[i];
        //   if (i>135 && i<=189) bandValues[13] += (int)vReal[i];
        //   if (i>189 && i<=264) bandValues[14] += (int)vReal[i];
        //   if (i>264          ) bandValues[15] += (int)vReal[i];

      }
    }

    // Process the FFT data into bar heights
    for (int band = 0; band < NUM_BANDS; band++) {

      // Scale the bars for the display
      int barHeight = bandValues[band] / AMPLITUDE;
      // if (barHeight > TOP) barHeight = TOP;

      // Small amount of averaging between frames
      if (USE_AVERAGING) {
        barHeight = ((oldBarHeights[band] * 1) + barHeight) / 2;
      }
      
      // Move peak up
      if (barHeight > peak[band]) {
        // peak[band] = min(TOP, barHeight);
        peak[band] = barHeight;
      }

      // Save oldBarHeights for averaging later
      oldBarHeights[band] = barHeight;
    }

    // // Decay peak
    // if (millis() - lastPeakUpdate > 20) {
    //   lastPeakUpdate = millis();

    // }

    for (byte band = 0; band < NUM_BANDS; band++) {
      if (peak[band] > 0) peak[band] -= PEAK_DECAY_RATE;
    }

    noiseLevel = (peak[0] + peak[1] + peak[2] + peak[3]);
    avgNoise = (NOISE_EMA_ALPHA * noiseLevel) + ((1 - NOISE_EMA_ALPHA) * avgNoise);
    // noiseLevel = (oldBarHeights[1] + oldBarHeights[2] + oldBarHeights[3] + oldBarHeights[4]); // / (AMPLITUDE / 4);

  #else
    noiseLevel = frame % 2 == 0 ? 0 : 100;
  #endif






  output += "noiseLevel: ";
  output += noiseLevel;
  output += "\n";


  // output += "Spectrogram: \n";
  // for (int band = 0; band < NUM_BANDS; band++) { 
  //   output += band;
  //   for(int i = 0; i < oldBarHeights[band]; i++) {
  //     output += ("=");
  //   }
  //   output += "\n";
  // }

  // output += "\n\n";


  // =====================================================================




  if (updateServo) {
    // Example servo movements
    servo1.write((int) (90 + 90 * (servo_master_speed * servo1_speed)));
    servo2.write((int) (90 + 90 * (servo_master_speed * servo2_speed)));
    servo3.write((int) (90 + 90 * (servo_master_speed * servo3_speed)));
    updateServo = false; // Prevent updating the servo in the next loop iteration
    Serial.println((int) (90 + 90 * (servo_master_speed * servo1_speed)) );
  }

  // Simple non-blocking delay for servo update
  if (millis() - lastServoUpdate > 1000/SERVO_UPDATE_HZ) { // 1-second delay between servo updates
    lastServoUpdate = millis();
    updateServo = true;
  }

  // pulseWhiteAndRGB();

  // whiteOverRainbow(); // Update LEDs

  loopyRainbow();

  applyWhiteBars(noiseLevel);

  strip.show();

  Serial.print(output);
  // delay(10);

}

