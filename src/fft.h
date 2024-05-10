#ifndef SPECTROGRAM_H
#define SPECTROGRAM_H

#define NUM_BANDS 8
#define SAMPLES 512

void setupAsyncSampling();
void setupI2S();
void computeSpectrogram(float spectrogram[NUM_BANDS]);
void applyKickDrumIsolationFilter(float spectrumFiltered[SAMPLES / 2]);
float calculateEntropyChange(float spectrumFiltered[SAMPLES / 2], float spectrumFilteredPrev[SAMPLES / 2]);

#endif