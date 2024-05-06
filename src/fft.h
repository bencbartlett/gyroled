#ifndef SPECTROGRAM_H
#define SPECTROGRAM_H

#define NUM_BANDS 8

void setupI2S();
void computeSpectrogram(int spectrogram[NUM_BANDS]);

#endif