#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_task_wdt.h"
#include <Arduino.h>
#include <arduinoFFT.h>
#include <driver/i2s.h>
#include <cmath> 

#define AUDIO_IN_PIN 35

#define SAMPLES 512          // CAN"T CHANGE AT THE MOMENT, Must be a power of 2
// #define SAMPLING_FREQ 40000 // Hz, must be 40000 or less due to ADC conversion time. Determines maximum frequency that can be analysed by the FFT Fmax=sampleF/2.
#define SAMPLING_FREQ 23000
// #define SAMPLING_FREQ 5900
#define AMPLITUDE 100      // Depending on your audio source level, you may need to alter this value. Can be used as a 'sensitivity' control.
#define NUM_BANDS 8         // To change this, you will need to change the bunch of if statements describing the mapping from bins to bands
#define NOISE 200            // Used as a crude noise filter, values below this are ignored, should be at least 10x amplitude
#define USE_ROLLING_AMPLITUDE_AVERAGING false
#define PEAK_DECAY_RATE 1
#define NOISE_EMA_ALPHA 0.02 
#define USE_AVERAGING true
#define SPECTRUM_EMA_ALPHA 0.7

#define USE_RAW_ADC_READ true
#define ADC_CHANNEL ADC1_CHANNEL_7 // Assume microphone is connected to GPIO35 (ADC1_CHANNEL_7)
#define ADC_WIDTH ADC_WIDTH_BIT_12
#define ADC_ATTEN ADC_ATTEN_DB_11

#define USE_INMP441 false

#define I2S_NUM         I2S_NUM_0  // I2S port number
#define I2S_SAMPLE_RATE     44100      // Sample rate of the microphone
#define I2S_SAMPLE_SIZE     32         // Bits per sample
#define BUFFER_LENGTH   512        // Length of the cyclic buffer for reading data; must be at least SAMPLES length

#define I2S_WS  12         // aka LRCL
#define I2S_SD  33         // aka DOUT
#define I2S_SCK 13         // aka BCLK

// Sampling and FFT stuff
const unsigned int sampling_period_us = round(1000000. / SAMPLING_FREQ);
const unsigned int sampling_period_ms = round(1000. / SAMPLING_FREQ);
const float scaleFactor = float(SAMPLES / (2.0 * 256.0)); // Calculate the scale factor based on new SAMPLES

// The nth frequency bin has frequency = (n) * (sampling frequency / 2) / (number of samples)
const float binFrequencySize = (SAMPLING_FREQ / 2.0) / SAMPLES;

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
// float oldBarHeights[NUM_BANDS] = {}; // Initializes to all zeros
float bandValues[NUM_BANDS] = {};
float avgBandValues[NUM_BANDS] = {};


float vReal[SAMPLES] = {};
float vImag[SAMPLES] = {};


const float kick_hz_mu = 80.0; // Center frequency in Hz
const float kick_hz_sigma = 80.0; // Standard deviation in Hz

float currentBandEnergy[NUM_BANDS];
float lastBandEnergy[NUM_BANDS];

volatile uint16_t buffer[SAMPLES];
volatile uint16_t lastIndex = 0;



// int frame = 0;
unsigned long newTime;

// int32_t buffer[SAMPLES];
// size_t bytes_read;


ArduinoFFT<float> FFT = ArduinoFFT<float>(vReal, vImag, SAMPLES, SAMPLING_FREQ);

void setupI2S() {
    // Serial.begin(115200);

    // The I2S config as per the example
    const i2s_config_t i2s_config = {
        .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX), // Receive, not transfer
        .sample_rate = I2S_SAMPLE_RATE,
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

static void async_sampling(void* arg) {
# if USE_RAW_ADC_READ
    while (true) {
        // buffer[lastIndex] = analogRead(AUDIO_IN_PIN);  // On ESP32-DevKitC core 1 has a throughput of about 5995 samples/s
        buffer[lastIndex] = adc1_get_raw(ADC_CHANNEL); // On ESP32-DevKitC core 1 has a throughput of about 23569.49 samples/s
        lastIndex = (lastIndex + 1) % BUFFER_LENGTH;
        // if (lastIndex == 0) {
        //     // Serial.print("Samples/sec: ");
        //     // Serial.println(1000000.0 * BUFFER_LENGTH / (micros() - newTime));
        //     // newTime = micros();
        //     vTaskDelay(1); // this keeps the watchdog happy
        // }
    }
# else
    while (true) {
        buffer[lastIndex] = analogRead(AUDIO_IN_PIN);  // On ESP32-DevKitC core 1 has a throughput of about 5995 samples/s
        // buffer[lastIndex] = adc1_get_raw(ADC_CHANNEL); // On ESP32-DevKitC core 1 has a throughput of about 23569.49 samples/s
        lastIndex = (lastIndex + 1) % BUFFER_LENGTH;
        // if (lastIndex == 0) {
        //     // Serial.print("Samples/sec: ");
        //     // Serial.println(1000000.0 * BUFFER_LENGTH / (micros() - newTime));
        //     // newTime = micros();
        //     vTaskDelay(1); // this keeps the watchdog happy
        // }
    }
#endif
}

void setupAsyncSampling() {
#if USE_RAW_ADC_READ
    adc1_config_width(ADC_WIDTH);
    adc1_config_channel_atten(ADC_CHANNEL, ADC_ATTEN);
#endif
    TaskHandle_t samplingTaskHandle;
    xTaskCreatePinnedToCore(
        async_sampling,
        "async_sampling",
        2048,
        NULL,
        1,
        &samplingTaskHandle,
        0
    );
    esp_task_wdt_delete(samplingTaskHandle);
    esp_task_wdt_delete(xTaskGetIdleTaskHandleForCPU(0));
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

    // // Sample the audio pin
    // for (int i = 0; i < SAMPLES; i++) {
    //     newTime = micros();
    //     vReal[i] = analogRead(AUDIO_IN_PIN); // A conversion takes about 9.7uS on an ESP32
    //     vImag[i] = 0.0;
    //     while ((micros() - newTime) < sampling_period_us) { /* chill */ }
    // }

    int localIndex = lastIndex;
    for (int i = 0; i < SAMPLES; i++) {
        vReal[i] = buffer[(localIndex + i) % BUFFER_LENGTH];
        vImag[i] = 0.0;
    }

    float endSampleTime = micros() / 1000.0;
    // Serial.print("Sample time: ");
    // Serial.println(endSampleTime - startSampleTime);
#endif

    // float startFFTTime = micros() / 1000.0;

    FFT.dcRemoval();
    FFT.windowing(FFTWindow::Hamming, FFTDirection::Forward);
    FFT.compute(FFTDirection::Forward);
    FFT.complexToMagnitude();
    // float x = FFT.majorPeak();

    // Analyse FFT results
    for (uint16_t i = 2; i < (SAMPLES >> 1); i++) {
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




    // // Store in unused high frequency bands
    // bandValues[7] = 0.0;
    // for (uint16_t i = 2; i < (SAMPLES >> 1); i++) {
    //     bandValues[7] += spectrumFiltered[i];
    // }
    // Serial.println(bandValues[6]);

    // Process the FFT data into bar heights
    for (int band = 0; band < NUM_BANDS; band++) {

        // Scale the bars for the display
        float barHeight = bandValues[band] / AMPLITUDE;
        // if (barHeight > TOP) barHeight = TOP;

        // Exponential moving averaging between frames
        if (USE_AVERAGING) {
            avgBandValues[band] = (barHeight * SPECTRUM_EMA_ALPHA) + (avgBandValues[band] * (1.0 - SPECTRUM_EMA_ALPHA));
            spectrogram[band] = avgBandValues[band];
        }
        else {
            spectrogram[band] = barHeight;
        }
    }

    // float endFFTTime = micros() / 1000.0;
    // Serial.print("FFT time: ");
    // Serial.println(endFFTTime - startFFTTime);

}





void applyKickDrumIsolationFilter(float spectrumFiltered[SAMPLES / 2]) {
    for (uint16_t i = 2; i < (SAMPLES >> 1); i++) { // only first half of samples are used up to Nyquist frequency
        const float hz = i * binFrequencySize;
        // Kick drums have a fundamental frequency range around 80Hz so we'll weight these higher
        spectrumFiltered[i] = exp(-0.5 * pow((hz - kick_hz_mu) / kick_hz_sigma, 2)) * vReal[i] * vReal[i];
    }
}

float calculateEntropyChange(float spectrumFiltered[SAMPLES / 2], float spectrumFilteredPrev[SAMPLES / 2]) {
    float entropyChange = 0;
    for (uint16_t i = 2; i < (SAMPLES >> 1); i++) {
        entropyChange += abs(spectrumFiltered[i] - spectrumFilteredPrev[i]);
    }
    memcpy(spectrumFilteredPrev, spectrumFiltered, sizeof(spectrumFiltered));
    return entropyChange;
}

// float computeBeatHeuristic(float spectrogram[NUM_BANDS]) {

//     float entropyChange = 0;

//     for (uint16_t i = 2; i < (SAMPLES >> 1); i++) { // only first half of samples are used up to Nyquist frequency
//         entropyChange += abs(spectrumFiltered[i] - spectrumFilteredPrev[i]);
//     }




//     // // Calculate band energies and detect beat

//     // calculateBandEnergies(spectrogram, currentBandEnergy);
//     // float entropyChange = calculateEntropyChange(currentBandEnergy, lastBandEnergy);

//     // // if (entropy > entropyThreshold) {
//     // //     Serial.println("Beat detected!");
//     // // }

//     // // Store current energies for the next loop iteration
//     // memcpy(lastBandEnergy, currentBandEnergy, sizeof(lastBandEnergy));

//     return entropyChange;
// }
