#include <Arduino.h>
#include <arduinoFFT.h>
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


#define SAMPLES         512           // CAN"T CHANGE AT THE MOMENT, Must be a power of 2
#define SAMPLING_FREQ   40000         // Hz, must be 40000 or less due to ADC conversion time. Determines maximum frequency that can be analysed by the FFT Fmax=sampleF/2.
#define AMPLITUDE       1000          // Depending on your audio source level, you may need to alter this value. Can be used as a 'sensitivity' control.
#define NUM_BANDS       8             // To change this, you will need to change the bunch of if statements describing the mapping from bins to bands
#define NOISE           100           // Used as a crude noise filter, values below this are ignored
// #define TOP             16
#define USE_AVERAGING   false
#define USE_ROLLING_AMPLITUDE_AVERAGING false


// Sampling and FFT stuff
unsigned int sampling_period_us;
int bandLimits[8] = {3, 6, 13, 27, 55, 112, 229};
double scaleFactor = (double)(SAMPLES/2) / 256.0; // Calculate the scale factor based on new SAMPLES

int peak[SAMPLES];              // The length of these arrays must be >= NUM_BANDS
int oldBarHeights[SAMPLES];
int bandValues[SAMPLES];
float vReal[SAMPLES];
float vImag[SAMPLES];
int noiseLevel;
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

  Serial.begin(115200);

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



void rainbowFade2White(int wait, int rainbowLoops, int whiteLoops) {
  int fadeVal=0, fadeMax=100;
  fadeVal = 100;

  // Hue of first pixel runs 'rainbowLoops' complete loops through the color
  // wheel. Color wheel has a range of 65536 but it's OK if we roll over, so
  // just count from 0 to rainbowLoops*65536, using steps of 256 so we
  // advance around the wheel at a decent clip.
  for(uint32_t firstPixelHue = 0; firstPixelHue < rainbowLoops*65536; firstPixelHue += 256) {

    for(int i=0; i<strip.numPixels(); i++) { // For each pixel in strip...

      // Offset pixel hue by an amount to make one full revolution of the
      // color wheel (range of 65536) along the length of the strip
      // (strip.numPixels() steps):
      uint32_t pixelHue = firstPixelHue + (i * 65536L / strip.numPixels());

      // strip.ColorHSV() can take 1 or 3 arguments: a hue (0 to 65535) or
      // optionally add saturation and value (brightness) (each 0 to 255).
      // Here we're using just the three-argument variant, though the
      // second value (saturation) is a constant 255.
      strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(pixelHue, 255,
        255 * fadeVal / fadeMax)));
    }

    // strip.show();
    // delay(wait);

    // if(firstPixelHue < 65536) {                              // First loop,
    //   if(fadeVal < fadeMax) fadeVal++;                       // fade in
    // } else if(firstPixelHue >= ((rainbowLoops-1) * 65536)) { // Last loop,
    //   if(fadeVal > 0) fadeVal--;                             // fade out
    // } else {
    //   fadeVal = fadeMax; // Interim loop, make sure fade is at max
    // }
  }

  // for(int k=0; k<whiteLoops; k++) {
  //   for(int j=0; j<256; j++) { // Ramp up 0 to 255
  //     // Fill entire strip with white at gamma-corrected brightness level 'j':
  //     strip.fill(strip.Color(0, 0, 0, strip.gamma8(j)));
  //     strip.show();
  //   }
  //   delay(1000); // Pause 1 second
  //   for(int j=255; j>=0; j--) { // Ramp down 255 to 0
  //     strip.fill(strip.Color(0, 0, 0, strip.gamma8(j)));
  //     strip.show();
  //   }
  // }

  // delay(500); // Pause 1/2 second
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

  String output = "";

  updatesPerSecond = 1000.0 / float(millis() - lastUpdate);
  lastUpdate = millis();

  output += "Updates per second: ";
  output += updatesPerSecond;
  output += "\n";

  // if (millis() - lastSample > 10) {
  if (true) {
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

        // 8 bands, 12kHz top band
        if (i<=3 )                bandValues[0]  += (int)vReal[i];
        else if (i>3   && i<=6  ) bandValues[1]  += (int)vReal[i];
        else if (i>6   && i<=13 ) bandValues[2]  += (int)vReal[i];
        else if (i>13  && i<=27 ) bandValues[3]  += (int)vReal[i];
        else if (i>27  && i<=55 ) bandValues[4]  += (int)vReal[i];
        else if (i>55  && i<=112) bandValues[5]  += (int)vReal[i];
        else if (i>112 && i<=229) bandValues[6]  += (int)vReal[i];
        else if (i>229          ) bandValues[7]  += (int)vReal[i];

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

    // Decay peak
    if (millis() - lastPeakUpdate > 20) {
      lastPeakUpdate = millis();
      for (byte band = 0; band < NUM_BANDS; band++) {
        if (peak[band] > 0) peak[band] -= 1;
      }
    }

    


  }

  noiseLevel = peak[1] + peak[2] + peak[3] + peak[4] / AMPLITUDE;
  output += "noiseLevel: ";
  output += noiseLevel;
  output += "\n";






  output += "Spectrogram: \n";
  for (int band = 0; band < NUM_BANDS; band++) { 
    output += band;
    for(int i = 0; i < oldBarHeights[band]; i++) {
      output += ("=");
    }
    output += "\n";
  }

  output += "\n\n";
  // Serial.println(noiseLevel);

  // =====================================================================




  if (updateServo) {
    // Example servo movements
    servo1.write(0);
    servo2.write(0);
    servo3.write(0);
    updateServo = false; // Prevent updating the servo in the next loop iteration
    // Serial.println("update servo");
  }

  // Simple non-blocking delay for servo update
  if (millis() - lastServoUpdate > 1000/SERVO_UPDATE_HZ) { // 1-second delay between servo updates
    lastServoUpdate = millis();
    updateServo = true;
  }

  // pulseWhiteAndRGB();

  whiteOverRainbow(); // Update LEDs

  // rainbowFade2White(3, 3, 1);

  for(int i=0; i<noiseLevel; i++) { 
    strip.setPixelColor(i, strip.Color(255, 255, 255, 255)); 
  }

  strip.show();

  Serial.print(output);
  // delay(10);

}

