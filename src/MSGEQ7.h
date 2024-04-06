#ifndef MSGEQ7_SPECTROGRAM_H
#define MSGEQ7_SPECTROGRAM_H

#include <stdint.h> // For uint16_t type

#define MAX_BAND 7 // Assuming MAX_BAND is a constant. If it's variable, adjust accordingly.

void recordSpectrogramMSGEQ7(uint16_t bandValues[MAX_BAND]);
void recordSpectrogramMSGEQ7_v2(uint16_t bandValues[MAX_BAND]);
void printSpectrogramMSGEQ7(uint16_t bandValues[MAX_BAND]);
void setup_MSGEQ7();
void setup_MSGEQ7_v2();

#endif // MSGEQ7_SPECTROGRAM_H
