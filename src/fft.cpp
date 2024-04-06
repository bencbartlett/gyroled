#include <Arduino.h>
#include <arduinoFFT.h>

#define AUDIO_IN_PIN 35

#define SAMPLES 256          // CAN"T CHANGE AT THE MOMENT, Must be a power of 2
#define SAMPLING_FREQ 40000 // Hz, must be 40000 or less due to ADC conversion time. Determines maximum frequency that can be analysed by the FFT Fmax=sampleF/2.
#define AMPLITUDE 1000      // Depending on your audio source level, you may need to alter this value. Can be used as a 'sensitivity' control.
#define NUM_BANDS 7         // To change this, you will need to change the bunch of if statements describing the mapping from bins to bands
#define NOISE 0            // Used as a crude noise filter, values below this are ignored, should be at least 10x amplitude
#define USE_AVERAGING true
#define USE_ROLLING_AMPLITUDE_AVERAGING false
#define PEAK_DECAY_RATE 1
#define NOISE_EMA_ALPHA 0.02 // At about 25fps this is about a 2 second window

// Sampling and FFT stuff
const unsigned int sampling_period_us = round(1000000. / SAMPLING_FREQ);
const double scaleFactor = (double)(SAMPLES / 2) / 256.0; // Calculate the scale factor based on new SAMPLES

// The nth frequency bin has frequency = (n) * (sampling frequency) / (number of samples)
const float binFrequencySize = SAMPLING_FREQ / SAMPLES;

// The MGSEQ7 has bands at 63Hz, 160Hz, 400Hz, 1kHz, 2.5kHz, 6.25kHz, 16kHz
const int bandsHz[NUM_BANDS] = {
    63,
    160,
    400,
    1000,
    2500,
    6250,
    16000
};
const int bandLimitsHz[NUM_BANDS-1] = {
    (bandsHz[0] + bandsHz[1]) / 2,
    (bandsHz[1] + bandsHz[2]) / 2,
    (bandsHz[2] + bandsHz[3]) / 2,
    (bandsHz[3] + bandsHz[4]) / 2,
    (bandsHz[4] + bandsHz[5]) / 2,
    (bandsHz[5] + bandsHz[6]) / 2
}; 
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
    for (uint16_t i = 1; i < (SAMPLES >> 1); i++) { 
        if (vReal[i] > NOISE) {

            const float hz = i * binFrequencySize;
            // if (hz <= bandLimitsHz[0])                              bandValues[0] += (int)vReal[i];
            // else if (hz > bandLimitsHz[0] && hz <= bandLimitsHz[1]) bandValues[1] += (int)vReal[i];
            // else if (hz > bandLimitsHz[1] && hz <= bandLimitsHz[2]) bandValues[2] += (int)vReal[i];
            // else if (hz > bandLimitsHz[2] && hz <= bandLimitsHz[3]) bandValues[3] += (int)vReal[i];
            // else if (hz > bandLimitsHz[3] && hz <= bandLimitsHz[4]) bandValues[4] += (int)vReal[i];
            // else if (hz > bandLimitsHz[4] && hz <= bandLimitsHz[5]) bandValues[5] += (int)vReal[i];
            // else if (hz > bandLimitsHz[5] && hz <= bandLimitsHz[6]) bandValues[6] += (int)vReal[i];
            // else if (hz > bandLimitsHz[6])                          bandValues[7] += (int)vReal[i];

            if (hz <= bandLimitsHz[0])                              bandValues[0] += (int)vReal[i];
            else if (hz > bandLimitsHz[0] && hz <= bandLimitsHz[1]) bandValues[1] += (int)vReal[i];
            else if (hz > bandLimitsHz[1] && hz <= bandLimitsHz[2]) bandValues[2] += (int)vReal[i];
            else if (hz > bandLimitsHz[2] && hz <= bandLimitsHz[3]) bandValues[3] += (int)vReal[i];
            else if (hz > bandLimitsHz[3] && hz <= bandLimitsHz[4]) bandValues[4] += (int)vReal[i];
            else if (hz > bandLimitsHz[4] && hz <= bandLimitsHz[5]) bandValues[5] += (int)vReal[i];
            else if (hz > bandLimitsHz[6])                          bandValues[6] += (int)vReal[i];

        }
    }
    
    uint16_t i = 0;

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
