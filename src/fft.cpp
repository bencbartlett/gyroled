#include <Arduino.h>
#include <arduinoFFT.h>
#include <driver/i2s.h>

#define AUDIO_IN_PIN 35

#define SAMPLES 512          // CAN"T CHANGE AT THE MOMENT, Must be a power of 2
#define SAMPLING_FREQ 40000 // Hz, must be 40000 or less due to ADC conversion time. Determines maximum frequency that can be analysed by the FFT Fmax=sampleF/2.
#define AMPLITUDE 100      // Depending on your audio source level, you may need to alter this value. Can be used as a 'sensitivity' control.
#define NUM_BANDS 7         // To change this, you will need to change the bunch of if statements describing the mapping from bins to bands
#define NOISE 0            // Used as a crude noise filter, values below this are ignored, should be at least 10x amplitude
#define USE_AVERAGING true
#define USE_ROLLING_AMPLITUDE_AVERAGING false
#define PEAK_DECAY_RATE 1
#define NOISE_EMA_ALPHA 0.02 // At about 25fps this is about a 2 second window
#define DO_FFT true


#define USE_INMP441 false

#define I2S_NUM         I2S_NUM_0  // I2S port number
#define SAMPLE_RATE     44100      // Sample rate of the microphone
#define SAMPLE_SIZE     32         // Bits per sample
// #define BUFFER_LENGTH   256        // Length of the buffer for reading data

#define I2S_WS  12         // aka LRCL
#define I2S_SD  33         // aka DOUT
#define I2S_SCK 13         // aka BCLK

// Sampling and FFT stuff
const unsigned int sampling_period_us = round(1000000. / SAMPLING_FREQ);
const float scaleFactor = float(SAMPLES / (2.0 * 256.0)); // Calculate the scale factor based on new SAMPLES

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
const int bandLimitsHz[NUM_BANDS - 1] = {
    (bandsHz[0] + bandsHz[1]) / 2,
    (bandsHz[1] + bandsHz[2]) / 2,
    (bandsHz[2] + bandsHz[3]) / 2,
    (bandsHz[3] + bandsHz[4]) / 2,
    (bandsHz[4] + bandsHz[5]) / 2,
    (bandsHz[5] + bandsHz[6]) / 2
};
// const int bandLimitsHz[NUM_BANDS - 1] = {
//     (bandsHz[0]),
//     (bandsHz[1]),
//     (bandsHz[2]),
//     (bandsHz[3]),
//     (bandsHz[4]),
//     (bandsHz[5]) //,
//     // (bandsHz[6]) / 2
// };
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
float oldBarHeights[NUM_BANDS] = {}; // Initializes to all zeros
float bandValues[NUM_BANDS] = {};
float vReal[SAMPLES];
float vImag[SAMPLES];

float currentBandEnergy[NUM_BANDS];
float lastBandEnergy[NUM_BANDS];

// int frame = 0;
unsigned long newTime;

int32_t buffer[SAMPLES];
size_t bytes_read;


ArduinoFFT<float> FFT = ArduinoFFT<float>(vReal, vImag, SAMPLES, SAMPLING_FREQ);

void setupI2S() {
    // Serial.begin(115200);

    // The I2S config as per the example
    const i2s_config_t i2s_config = {
        .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX), // Receive, not transfer
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_STAND_I2S),
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = SAMPLES,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0,
    };
    // The pin config as per the setup
    const i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_SCK,       // BCLK aka SCK
        .ws_io_num = I2S_WS,        // LRCL aka WS
        .data_out_num = -1,         // not used (only for speakers)
        .data_in_num = I2S_SD       // DOUT aka SD
    };

    // Installing the I2S driver
    i2s_driver_install(I2S_NUM, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_NUM, &pin_config);

    // esp_err_t err;
    // err = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
    // if (err != ESP_OK) {
    //     Serial.printf("Failed installing driver: %d\n", err);
    //     while (true);
    // }
    // err = i2s_set_pin(I2S_PORT, &pin_config);
    // if (err != ESP_OK) {
    //     Serial.printf("Failed setting pin: %d\n", err);
    //     while (true);
    // }
    // Serial.println("I2S driver installed.");
    // delay(100);
}

void computeSpectrogram(float spectrogram[NUM_BANDS]) {


    for (int i = 0; i < NUM_BANDS; i++) {
        bandValues[i] = 0;
    }

#if USE_INMP441
    i2s_read(I2S_NUM, &buffer, sizeof(buffer), &bytes_read, portMAX_DELAY);

    for (int i = 0; i < SAMPLES; i++) {
        vReal[i] = (float)abs(buffer[i] >> 16); // Shift and cast to 16-bit integer
        // vReal[i] = (float)(buffer[i]);
        vImag[i] = 0.0;
    }
#else
    float startSampleTime = micros() / 1000.0;

    // Sample the audio pin
    for (int i = 0; i < SAMPLES; i++) {
        newTime = micros();
        vReal[i] = analogRead(AUDIO_IN_PIN); // A conversion takes about 9.7uS on an ESP32
        vImag[i] = 0.0;
        while ((micros() - newTime) < sampling_period_us) { /* chill */ }
    }

    float endSampleTime = micros() / 1000.0;
    Serial.print("Sample time: ");
    Serial.println(endSampleTime - startSampleTime);
#endif

#if DO_FFT

    float startFFTTime = micros() / 1000.0;

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

            if (hz <= bandLimitsHz[0])                              bandValues[0] += vReal[i];
            else if (hz > bandLimitsHz[0] && hz <= bandLimitsHz[1]) bandValues[1] += vReal[i];
            else if (hz > bandLimitsHz[1] && hz <= bandLimitsHz[2]) bandValues[2] += vReal[i];
            else if (hz > bandLimitsHz[2] && hz <= bandLimitsHz[3]) bandValues[3] += vReal[i];
            else if (hz > bandLimitsHz[3] && hz <= bandLimitsHz[4]) bandValues[4] += vReal[i];
            else if (hz > bandLimitsHz[4] && hz <= bandLimitsHz[5]) bandValues[5] += vReal[i];
            else if (hz > bandLimitsHz[5] && hz <= bandLimitsHz[6]) bandValues[6] += vReal[i];
            else if (hz > bandLimitsHz[6])                          bandValues[7] += vReal[i];
        }
    }

    bandValues[0] = 0.0;
    int bassMin = 20;
    int bassMax = int(150 * scaleFactor);
    for (uint16_t i = bassMin; i <= bassMax; i++) {
        bandValues[0] += vReal[i];
    }

#else 
    for (uint16_t i = 0; i < (SAMPLES >> 1); i++) {
        bandValues[3] += vReal[i];
    }
#endif

    // Process the FFT data into bar heights
    for (int band = 0; band < NUM_BANDS; band++) {

        // Scale the bars for the display
        float barHeight = bandValues[band] / AMPLITUDE;
        // if (barHeight > TOP) barHeight = TOP;

        // Small amount of averaging between frames
        if (USE_AVERAGING) {
            barHeight = ((oldBarHeights[band] * 1.0) + barHeight) / 2.0;
        }

        // Save oldBarHeights for averaging later
        oldBarHeights[band] = barHeight;
        spectrogram[band] = barHeight;
    }

    float endFFTTime = micros() / 1000.0;
    Serial.print("FFT time: ");
    Serial.println(endFFTTime - startFFTTime);

}

void calculateBandEnergies(float spectrogram[NUM_BANDS], float *bandEnergies) {
  for (int i = 0; i < NUM_BANDS; i++) {
    bandEnergies[i] = spectrogram[i] * spectrogram[i];
  }
}

float calculateEntropyChange(float* current, float* last) {
    float entropyChange = 0;
    for (int i = 0; i < NUM_BANDS; i++) {
        float diff = abs(current[i] - last[i]);
        entropyChange += diff;
    }
    return entropyChange;
}

float computeBeatHeuristic(float spectrogram[NUM_BANDS]) {
    // Calculate band energies and detect beat
    
    calculateBandEnergies(spectrogram, currentBandEnergy);
    float entropyChange = calculateEntropyChange(currentBandEnergy, lastBandEnergy);

    // if (entropy > entropyThreshold) {
    //     Serial.println("Beat detected!");
    // }

    // Store current energies for the next loop iteration
    memcpy(lastBandEnergy, currentBandEnergy, sizeof(lastBandEnergy));

    return entropyChange;
}
