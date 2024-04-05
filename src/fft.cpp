#include <Arduino.h>
#include <arduinoFFT.h>

#define AUDIO_IN_PIN 35

#define SAMPLES 64          // CAN"T CHANGE AT THE MOMENT, Must be a power of 2
#define SAMPLING_FREQ 10000 // Hz, must be 40000 or less due to ADC conversion time. Determines maximum frequency that can be analysed by the FFT Fmax=sampleF/2.
#define AMPLITUDE 1000      // Depending on your audio source level, you may need to alter this value. Can be used as a 'sensitivity' control.
#define NUM_BANDS 8         // To change this, you will need to change the bunch of if statements describing the mapping from bins to bands
#define NOISE 5000            // Used as a crude noise filter, values below this are ignored, should be at least 10x amplitude
#define USE_AVERAGING false
#define USE_ROLLING_AMPLITUDE_AVERAGING false
#define PEAK_DECAY_RATE 1
#define NOISE_EMA_ALPHA 0.02 // At about 25fps this is about a 2 second window

// Sampling and FFT stuff
const unsigned int sampling_period_us = round(1000000. / SAMPLING_FREQ);
const double scaleFactor = (double)(SAMPLES / 2) / 256.0; // Calculate the scale factor based on new SAMPLES

// The nth frequency bin has frequency (n) * (sampling frequency) / (number of samples)
const int binFrequencySize = SAMPLING_FREQ / SAMPLES;
const int bandLimits[7] = {
    int(3 * scaleFactor),
    int(6 * scaleFactor),
    int(13 * scaleFactor),
    int(27 * scaleFactor),
    int(55 * scaleFactor),
    int(112 * scaleFactor),
    int(229 * scaleFactor)
};

// int peak[NUM_BANDS] = {};          // The length of these arrays must be >= NUM_BANDS
int oldBarHeights[NUM_BANDS] = {}; // Initializes to all zeros
int bandValues[NUM_BANDS] = {};
float vReal[SAMPLES];
float vImag[SAMPLES];
// int frame = 0;
unsigned long newTime;

ArduinoFFT<float> FFT = ArduinoFFT<float>(vReal, vImag, SAMPLES, SAMPLING_FREQ);


void computeSpectrogram(int spectrogram[NUM_BANDS]) {

    for (int i = 0; i < NUM_BANDS; i++) {
        bandValues[i] = 0;
    }

    // Sample the audio pin
    for (int i = 0; i < SAMPLES; i++) {
        newTime = micros();
        vReal[i] = analogRead(AUDIO_IN_PIN); // A conversion takes about 9.7uS on an ESP32
        vImag[i] = 0.0;
        while ((micros() - newTime) < sampling_period_us) { /* chill */ }
    }

    FFT.dcRemoval();
    FFT.windowing(FFTWindow::Hamming, FFTDirection::Forward);
    FFT.compute(FFTDirection::Forward);
    FFT.complexToMagnitude();
    // float x = FFT.majorPeak();

    // Analyse FFT results
    for (int i = 2; i < (SAMPLES / 2); i++) { 
        // Don't use sample 0 and only first SAMPLES/2 are usable. Each array element represents a frequency bin and its value the amplitude.
        if (vReal[i] > NOISE) { // Add a crude noise filter

            // // 8 bands, 12kHz top band
            // if (i<=3 )                bandValues[0]  += (int)vReal[i];
            // else if (i>3   && i<=6  ) bandValues[1]  += (int)vReal[i];
            // else if (i>6   && i<=13 ) bandValues[2]  += (int)vReal[i];
            // else if (i>13  && i<=27 ) bandValues[3]  += (int)vReal[i];
            // else if (i>27  && i<=55 ) bandValues[4]  += (int)vReal[i];
            // else if (i>55  && i<=112) bandValues[5]  += (int)vReal[i];
            // else if (i>112 && i<=229) bandValues[6]  += (int)vReal[i];
            // else if (i>229          ) bandValues[7]  += (int)vReal[i];

            if (i <= bandLimits[0])                           bandValues[0] += (int)vReal[i];
            else if (i > bandLimits[0] && i <= bandLimits[1]) bandValues[1] += (int)vReal[i];
            else if (i > bandLimits[1] && i <= bandLimits[2]) bandValues[2] += (int)vReal[i];
            else if (i > bandLimits[2] && i <= bandLimits[3]) bandValues[3] += (int)vReal[i];
            else if (i > bandLimits[3] && i <= bandLimits[4]) bandValues[4] += (int)vReal[i];
            else if (i > bandLimits[4] && i <= bandLimits[5]) bandValues[5] += (int)vReal[i];
            else if (i > bandLimits[5] && i <= bandLimits[6]) bandValues[6] += (int)vReal[i];
            else if (i > bandLimits[6])                       bandValues[7] += (int)vReal[i];

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

        // Save oldBarHeights for averaging later
        oldBarHeights[band] = barHeight;
        spectrogram[band] = barHeight;
    }
}
