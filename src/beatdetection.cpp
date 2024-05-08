#include <Arduino.h>
#include <algorithm>  // Include for std::nth_element and std::sort
#include <cmath>      // Include for std::floor and std::ceil

#include "fft.h"


#define HEURISTIC_BUFFER_SIZE 2048  // at 30fps this is 68 seconds of context

float heuristicsBuffer[HEURISTIC_BUFFER_SIZE] = {};
uint16_t heuristicsBufferIndex = 0;
float bufferCopy[HEURISTIC_BUFFER_SIZE] = {};

float spectrumFiltered[SAMPLES / 2] = {};
float spectrumFilteredPrev[SAMPLES / 2] = {};

const int MAXIMUM_BEATS_PER_MINUTE = 180;
const int MINIMUM_DELAY_BETWEEN_BEATS = 60000L / MAXIMUM_BEATS_PER_MINUTE;
const int SINGLE_BEAT_DURATION = 100; // good value range is [50:150]

unsigned long lastBeatTimestamp = 0;

float calculateRecencyFactor() {
  unsigned long durationSinceLastBeat = millis() - lastBeatTimestamp;
  int referenceDuration = MINIMUM_DELAY_BETWEEN_BEATS - SINGLE_BEAT_DURATION;
  // float recencyFactor = constrain(1 - (float(referenceDuration) / durationSinceLastBeat), 0.0, 1.0);
  float recencyFactor = constrain(float(durationSinceLastBeat) / referenceDuration, 0.0, 1.0);
  return recencyFactor;
}


float computePercentile(float buffer[HEURISTIC_BUFFER_SIZE], float percentile) {
  // Calculate the index corresponding to the percentile
  int index = static_cast<int>(std::ceil(percentile / 100.0 * HEURISTIC_BUFFER_SIZE) - 1);
  index = std::max(0, std::min(index, HEURISTIC_BUFFER_SIZE - 1));
  // Create a copy of the buffer to avoid modifying the original
  memcpy(bufferCopy, buffer, sizeof(buffer));
  // Use std::nth_element to rearrange elements
  std::nth_element(bufferCopy, bufferCopy + index, bufferCopy + HEURISTIC_BUFFER_SIZE);
  // Return the element at the index
  return bufferCopy[index];
}


float computeBeatHeuristic(float fftResults[SAMPLES]) {

  applyKickDrumIsolationFilter(spectrumFiltered);
  float entropyChange = calculateEntropyChange(spectrumFiltered, spectrumFilteredPrev);
  float heuristic = entropyChange * calculateRecencyFactor();

  // If the heuristic is above some percentile of the buffer, we have a beat
  float fudgeFactor = 1.3;
  float percentile = (1.0 - fudgeFactor * (1./30. * MAXIMUM_BEATS_PER_MINUTE / 60.)) * 100;
  float heuristicThreshold = computePercentile(heuristicsBuffer, percentile);
  if (heuristic > heuristicThreshold) {
    lastBeatTimestamp = millis();
  }

  heuristicsBuffer[heuristicsBufferIndex] = heuristic;
  heuristicsBufferIndex = (heuristicsBufferIndex + 1) % HEURISTIC_BUFFER_SIZE;

  return heuristic;
}