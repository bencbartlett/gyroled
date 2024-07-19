#ifndef SPECTROGRAM_H
#define SPECTROGRAM_H

#define NUM_BANDS 8
#define SAMPLES 512
#define NUM_GEQ_CHANNELS 16

void setupAsyncSampling();
void setupI2S();
void computeSpectrogram(float spectrogram[NUM_BANDS]);
void doFFT(float frequencies[SAMPLES / 2]);
void computeSpectrogramWLED(float frequencies[SAMPLES / 2], float spectrogram[NUM_GEQ_CHANNELS]);
void postProcessFFTResults(float fftResults[NUM_GEQ_CHANNELS], float postProcessedResults[NUM_GEQ_CHANNELS]);
void applyKickDrumIsolationFilter(float frequencies[SAMPLES / 2], float spectrumFiltered[SAMPLES / 2]);
float calculateEntropyChange(float spectrumFiltered[SAMPLES / 2], float spectrumFilteredPrev[SAMPLES / 2]);

#endif