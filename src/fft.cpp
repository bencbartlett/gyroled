#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_task_wdt.h"
#include <Arduino.h>
#include <arduinoFFT.h>
#include <driver/i2s.h>
#include <cmath> 

#define AUDIO_IN_PIN 		35

// FFT parameters
#define SAMPLES 			512		// Must be a power of 2
#define BUFFER_LENGTH   	512		// Length of the cyclic buffer for reading data; must be at least SAMPLES length
// #define SAMPLING_FREQ 		23000	// Max sampling frequency of the mic if using adc1_get_raw()
# define SAMPLING_FREQ 	    22050
// #define SAMPLING_FREQ 	5900 	// Max sampling frequency of the mic if using analogRead()
#define AMPLITUDE 			100     // Audio amplitude scaling factor
#define NOISE 				200		// Can be used as a basic noise filter
#define NUM_BANDS 			8       // Number of bands in the frequency spectrogram
#define USE_AVERAGING 		true	// Whether to use exponential moving average to smooth out the noise levels
#define SPECTRUM_EMA_ALPHA 	0.7

#define USE_WLED_FFT        true    // Whether to use the WLED FFT algorithm
#define NUM_GEQ_CHANNELS 	16 

// ADC parameters
#define USE_RAW_ADC_READ 	true	// Whether to use adc1_get_raw() (faster) or analogRead() (slower) for sampling
#define ADC_CHANNEL 		ADC1_CHANNEL_7
#define ADC_WIDTH 			ADC_WIDTH_BIT_12
#define ADC_ATTEN 			ADC_ATTEN_DB_11

// Alternate INMP441 sampling scheme
#define USE_INMP441 		false	// Whether to use the INMP441 microphone or the default MAX4466
#define I2S_SAMPLE_RATE     44100   // Sample rate of the INMP441 mic
#define I2S_SAMPLE_SIZE     32      // Bits per sample
#define I2S_WS  			12      // aka LRCL
#define I2S_SD  			33      // aka DOUT
#define I2S_SCK 			13      // aka BCLK
#define I2S_NUM         	I2S_NUM_0

// WLED FFT params
static float fftResultPink[NUM_GEQ_CHANNELS] = { 1.70f, 1.71f, 1.73f, 1.78f, 1.68f, 1.56f, 1.55f, 1.63f, 1.79f, 1.62f, 1.80f, 2.06f, 2.47f, 3.35f, 6.83f, 9.55f }; // Table of multiplication factors so that we can even out the frequency response.
static uint8_t FFTScalingMode = 3;            // 0 none; 1 optimized logarithmic; 2 optimized linear; 3 optimized square root
#define FFT_DOWNSCALE 0.46f                             // downscaling factor for FFT results - for "Flat-Top" window @22Khz, new freq channels
#define LOG_256  5.54517744f
static uint8_t soundAgc = 1;                  // Automagic gain control: 0 - none, 1 - normal, 2 - vivid, 3 - lazy (config value)
static float    multAgc = 1.0f;                 // sample * multAgc = sampleAgc. Our AGC multiplier
uint8_t sampleGain = 60;
static uint8_t inputLevel = 128;              // UI slider value
static uint16_t decayTime = 1400;             // int: decay time in milliseconds.  Default 1.40sec
bool limiterOn = false;                        // limiter on/off

// Cyclic buffer for storing audio samples
volatile uint16_t buffer[SAMPLES];
volatile uint16_t lastIndex = 0;

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
const int bandLimitsHz[NUM_BANDS - 1] = {  // center-aligned
	(bandsHz[0] + bandsHz[1]) / 2,
	(bandsHz[1] + bandsHz[2]) / 2,
	(bandsHz[2] + bandsHz[3]) / 2,
	(bandsHz[3] + bandsHz[4]) / 2,
	(bandsHz[4] + bandsHz[5]) / 2,
	(bandsHz[5] + bandsHz[6]) / 2
};
// const int bandLimitsHz[NUM_BANDS - 1] = {  // right-aligned
//     (bandsHz[0]),
//     (bandsHz[1]),
//     (bandsHz[2]),
//     (bandsHz[3]),
//     (bandsHz[4]),
//     (bandsHz[5]) //,
//     // (bandsHz[6]) / 2
// };
const int bandLimits[7] = {  // band limits which are invariant under number of samples
	int(3 * scaleFactor),
	int(6 * scaleFactor),
	int(13 * scaleFactor),
	int(27 * scaleFactor),
	int(55 * scaleFactor),
	int(112 * scaleFactor),
	int(229 * scaleFactor)
};

#if USE_WLED_FFT
static float amplitudes[NUM_GEQ_CHANNELS] = {};
static float avgAmplitudes[NUM_GEQ_CHANNELS] = {};
#else
static float amplitudes[NUM_BANDS] = {};
static float avgAmplitudes[NUM_BANDS] = {};
#endif

static uint8_t fftResult[NUM_GEQ_CHANNELS] = { 0 };// Our calculated freq. channel result table to be used by effects

float avgFrequencyAmplitudes[SAMPLES / 2] = {};

float vReal[SAMPLES] = {};
float vImag[SAMPLES] = {};
ArduinoFFT<float> FFT = ArduinoFFT<float>(vReal, vImag, SAMPLES, SAMPLING_FREQ);

// Kick drum isolation filter
const float kick_hz_mu = 60.0; // Center frequency in Hz
const float kick_hz_sigma = 40.0; // Standard deviation in Hz

// For computing rolling average of the spectrum
float currentBandEnergy[NUM_BANDS];
float lastBandEnergy[NUM_BANDS];

// unsigned long newTime;


void setupI2S() {
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
		.bck_io_num = I2S_SCK,
		.ws_io_num = I2S_WS,
		.data_out_num = -1,
		.data_in_num = I2S_SD
	};
	// Installing the I2S driver
	i2s_driver_install(I2S_NUM, &i2s_config, 0, NULL);
	i2s_set_pin(I2S_NUM, &pin_config);
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


/**
 * Compute the spectrogram of the audio signal and bin frequencies into bands
 */
void computeSpectrogram(float spectrogram[NUM_BANDS]) {
	for (int i = 0; i < NUM_BANDS; i++) {
		amplitudes[i] = 0;
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
	// FFT.windowing(FFTWindow::Hamming, FFTDirection::Forward);
	FFT.windowing(FFTWindow::Flat_top, FFTDirection::Forward);  // flat top has better amplitude accuracy
	FFT.compute(FFTDirection::Forward);
	FFT.complexToMagnitude();

	// Analyse FFT results
	for (uint16_t i = 2; i < (SAMPLES >> 1); i++) {
		if (vReal[i] > NOISE) {

			const float hz = i * binFrequencySize;

			if (hz <= bandLimitsHz[0])                              amplitudes[0] += vReal[i];
			else if (hz > bandLimitsHz[0] && hz <= bandLimitsHz[1]) amplitudes[1] += vReal[i];
			else if (hz > bandLimitsHz[1] && hz <= bandLimitsHz[2]) amplitudes[2] += vReal[i];
			else if (hz > bandLimitsHz[2] && hz <= bandLimitsHz[3]) amplitudes[3] += vReal[i];
			else if (hz > bandLimitsHz[3] && hz <= bandLimitsHz[4]) amplitudes[4] += vReal[i];
			else if (hz > bandLimitsHz[4] && hz <= bandLimitsHz[5]) amplitudes[5] += vReal[i];
			else if (hz > bandLimitsHz[5] && hz <= bandLimitsHz[6]) amplitudes[6] += vReal[i];
			else if (hz > bandLimitsHz[6])                          amplitudes[7] += vReal[i];
		}
	}

	// Process the FFT data into bar heights
	for (int band = 0; band < NUM_BANDS; band++) {
		// Scale the bars for the display
		float barHeight = amplitudes[band] / AMPLITUDE;

		// Exponential moving averaging between frames
		if (USE_AVERAGING) {
			avgAmplitudes[band] = (barHeight * SPECTRUM_EMA_ALPHA) + (avgAmplitudes[band] * (1.0 - SPECTRUM_EMA_ALPHA));
			spectrogram[band] = avgAmplitudes[band];
		}
		else {
			spectrogram[band] = barHeight;
		}
	}

	// float endFFTTime = micros() / 1000.0;
	// Serial.print("FFT time: ");
	// Serial.println(endFFTTime - startFFTTime);
}

/**
 * Compute the FFT spectrum of the audio signal, keeping the first half of the samples
 */
void doFFT(float frequencies[SAMPLES / 2]) {

	int localIndex = lastIndex;
	for (int i = 0; i < SAMPLES; i++) {
		vReal[i] = buffer[(localIndex + i) % BUFFER_LENGTH];
		vImag[i] = 0.0;
	}

	FFT.dcRemoval();
	FFT.windowing(FFTWindow::Flat_top, FFTDirection::Forward);
	FFT.compute(FFTDirection::Forward);
	FFT.complexToMagnitude();

	// Accumulate FFT results
	frequencies[0] = 0.;
	frequencies[1] = 0.;
	for (uint16_t i = 2; i < (SAMPLES >> 1); i++) {
		// if (vReal[i] > NOISE) {
		frequencies[i] = vReal[i];
		// }
#if USE_AVERAGING 
		avgFrequencyAmplitudes[i] = (frequencies[i] * SPECTRUM_EMA_ALPHA) + (frequencies[i] * (1.0 - SPECTRUM_EMA_ALPHA));
		frequencies[i] = avgFrequencyAmplitudes[i];
#endif
	}

}

// float version of map()
static float mapf(float x, float in_min, float in_max, float out_min, float out_max) {
	return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

static float fftAddAvg(int from, int to) {
	float result = 0.0f;
	for (int i = from; i <= to; i++) {
		result += vReal[i];
	}
	return result / float(to - from + 1);
}


/**
 * A version of the FFT code ported from WLED's audio-reactive code
 */
void computeSpectrogramWLED(float spectrogram[NUM_BANDS]) {
	for (int i = 0; i < NUM_BANDS; i++) {
		amplitudes[i] = 0;
	}

	int localIndex = lastIndex;
	for (int i = 0; i < SAMPLES; i++) {
		vReal[i] = buffer[(localIndex + i) % BUFFER_LENGTH];
		vImag[i] = 0.0;
	}

	FFT.dcRemoval();
	// FFT.windowing(FFTWindow::Hamming, FFTDirection::Forward);
	FFT.windowing(FFTWindow::Flat_top, FFTDirection::Forward);  // flat top has better amplitude accuracy
	FFT.compute(FFTDirection::Forward);
	FFT.complexToMagnitude();

	amplitudes[0] = fftAddAvg(1, 2);               // 1    43 - 86   sub-bass
	amplitudes[1] = fftAddAvg(2, 3);               // 1    86 - 129  bass
	amplitudes[2] = fftAddAvg(3, 5);               // 2   129 - 216  bass
	amplitudes[3] = fftAddAvg(5, 7);               // 2   216 - 301  bass + midrange
	amplitudes[4] = fftAddAvg(7, 10);              // 3   301 - 430  midrange
	amplitudes[5] = fftAddAvg(10, 13);             // 3   430 - 560  midrange
	amplitudes[6] = fftAddAvg(13, 19);             // 5   560 - 818  midrange
	amplitudes[7] = fftAddAvg(19, 26);             // 7   818 - 1120 midrange -- 1Khz should always be the center !
	amplitudes[8] = fftAddAvg(26, 33);             // 7  1120 - 1421 midrange
	amplitudes[9] = fftAddAvg(33, 44);             // 9  1421 - 1895 midrange
	amplitudes[10] = fftAddAvg(44, 56);            // 12 1895 - 2412 midrange + high mid
	amplitudes[11] = fftAddAvg(56, 70);            // 14 2412 - 3015 high mid
	amplitudes[12] = fftAddAvg(70, 86);            // 16 3015 - 3704 high mid
	amplitudes[13] = fftAddAvg(86, 104);           // 18 3704 - 4479 high mid
	amplitudes[14] = fftAddAvg(104, 165) * 0.88f;  // 61 4479 - 7106 high mid + high  -- with slight damping
	amplitudes[15] = fftAddAvg(165, 215) * 0.70f;  // 50 7106 - 9259 high             -- with some damping

	// Process the FFT data into bar heights
	for (int band = 0; band < NUM_BANDS; band++) {
		// Scale the bars for the display
		float barHeight = amplitudes[band] / AMPLITUDE;

		// Exponential moving averaging between frames
		if (USE_AVERAGING) {
			avgAmplitudes[band] = (barHeight * SPECTRUM_EMA_ALPHA) + (avgAmplitudes[band] * (1.0 - SPECTRUM_EMA_ALPHA));
			spectrogram[band] = avgAmplitudes[band];
		}
		else {
			spectrogram[band] = barHeight;
		}
	}

	// float endFFTTime = micros() / 1000.0;
	// Serial.print("FFT time: ");
	// Serial.println(endFFTTime - startFFTTime);
}

/**
 * WLED's post-processing routine
 */
static void postProcessFFTResults() {
	for (int i = 0; i < NUM_GEQ_CHANNELS; i++) {

		if (true) { // noise gate open
			// Adjustment for frequency curves.
			amplitudes[i] *= fftResultPink[i];
			if (FFTScalingMode > 0) amplitudes[i] *= FFT_DOWNSCALE;  // adjustment related to FFT windowing function
			// Manual linear adjustment of gain using sampleGain adjustment for different input types.
			amplitudes[i] *= soundAgc ? multAgc : ((float)sampleGain / 40.0f * (float)inputLevel / 128.0f + 1.0f / 16.0f); //apply gain, with inputLevel adjustment
			if (amplitudes[i] < 0) amplitudes[i] = 0;
		}

		// smooth results - rise fast, fall slower
		if (amplitudes[i] > avgAmplitudes[i])   // rise fast 
			avgAmplitudes[i] = amplitudes[i] * 0.75f + 0.25f * avgAmplitudes[i];  // will need approx 2 cycles (50ms) for converging against amplitudes[i]
		else {                       // fall slow
			if (decayTime < 1000) avgAmplitudes[i] = amplitudes[i] * 0.22f + 0.78f * avgAmplitudes[i];       // approx  5 cycles (225ms) for falling to zero
			else if (decayTime < 2000) avgAmplitudes[i] = amplitudes[i] * 0.17f + 0.83f * avgAmplitudes[i];  // default - approx  9 cycles (225ms) for falling to zero
			else if (decayTime < 3000) avgAmplitudes[i] = amplitudes[i] * 0.14f + 0.86f * avgAmplitudes[i];  // approx 14 cycles (350ms) for falling to zero
			else avgAmplitudes[i] = amplitudes[i] * 0.1f + 0.9f * avgAmplitudes[i];                         // approx 20 cycles (500ms) for falling to zero
		}
		// constrain internal vars - just to be sure
		amplitudes[i] = constrain(amplitudes[i], 0.0f, 1023.0f);
		avgAmplitudes[i] = constrain(avgAmplitudes[i], 0.0f, 1023.0f);

		float currentResult;
		if (limiterOn == true)
			currentResult = avgAmplitudes[i];
		else
			currentResult = amplitudes[i];

		switch (FFTScalingMode) {
		case 1:
			// Logarithmic scaling
			currentResult *= 0.42f;                      // 42 is the answer ;-)
			currentResult -= 8.0f;                       // this skips the lowest row, giving some room for peaks
			if (currentResult > 1.0f) currentResult = logf(currentResult); // log to base "e", which is the fastest log() function
			else currentResult = 0.0f;                   // special handling, because log(1) = 0; log(0) = undefined
			currentResult *= 0.85f + (float(i) / 18.0f);  // extra up-scaling for high frequencies
			currentResult = mapf(currentResult, 0, LOG_256, 0, 255); // map [log(1) ... log(255)] to [0 ... 255]
			break;
		case 2:
			// Linear scaling
			currentResult *= 0.30f;                     // needs a bit more damping, get stay below 255
			currentResult -= 4.0f;                       // giving a bit more room for peaks
			if (currentResult < 1.0f) currentResult = 0.0f;
			currentResult *= 0.85f + (float(i) / 1.8f);   // extra up-scaling for high frequencies
			break;
		case 3:
			// square root scaling
			currentResult *= 0.38f;
			currentResult -= 6.0f;
			if (currentResult > 1.0f) currentResult = sqrtf(currentResult);
			else currentResult = 0.0f;                   // special handling, because sqrt(0) = undefined
			currentResult *= 0.85f + (float(i) / 4.5f);   // extra up-scaling for high frequencies
			currentResult = mapf(currentResult, 0.0, 16.0, 0.0, 255.0); // map [sqrt(1) ... sqrt(256)] to [0 ... 255]
			break;

		case 0:
		default:
			// no scaling - leave freq bins as-is
			currentResult -= 4; // just a bit more room for peaks
			break;
		}

		// Now, let's dump it all into fftResult. Need to do this, otherwise other routines might grab fftResult values prematurely.
		if (soundAgc > 0) {  // apply extra "GEQ Gain" if set by user
			float post_gain = (float)inputLevel / 128.0f;
			if (post_gain < 1.0f) post_gain = ((post_gain - 1.0f) * 0.8f) + 1.0f;
			currentResult *= post_gain;
		}
		fftResult[i] = constrain((int)currentResult, 0, 255);
		//   fftResult[i] = constrain(currentResult, 0.0f, 255.0f);
	}
}


float* applyKickDrumIsolationFilter(float frequencies[SAMPLES / 2]) {
	float spectrumFiltered[SAMPLES >> 1];
	for (uint16_t i = 0; i < (SAMPLES >> 1); i++) { // only first half of samples are used up to Nyquist frequency
		const float hz = i * binFrequencySize;
		// Kick drums have a fundamental frequency range around 80Hz so we'll weight these higher
		spectrumFiltered[i] = exp(-0.5 * pow((hz - kick_hz_mu) / kick_hz_sigma, 2)) * frequencies[i];
	}
	return spectrumFiltered;
}

float calculateEntropyChange(float spectrumFiltered[SAMPLES / 2], float spectrumFilteredPrev[SAMPLES / 2]) {
	float entropyChange = 0;
	for (uint16_t i = 2; i < (SAMPLES >> 1); i++) {
		entropyChange += abs(spectrumFiltered[i] - spectrumFilteredPrev[i]);
	}
	memcpy(spectrumFilteredPrev, spectrumFiltered, sizeof(spectrumFiltered));
	return entropyChange;
}
