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

const int MAXIMUM_BEATS_PER_MINUTE = 155;
const int TYPICAL_BEATS_PER_MINUTE = 126;
const int MINIMUM_DELAY_BETWEEN_BEATS = 60000L / MAXIMUM_BEATS_PER_MINUTE;
const int SINGLE_BEAT_DURATION = 100; // good value range is [50:150]

unsigned long lastBeatTimestamp = 0;
unsigned long elapsedBeats = 0;

const float alpha_short = 1.0 / (30.0 * 5.0); // roughly 5 seconds
const float alpha_long = 1.0 / (30.0 * 60.0); // roughly 60 seconds
float heuristic_ema = 1.0;

bool bufferInitialized = false;

float calculateRecencyFactor() {
	unsigned long durationSinceLastBeat = millis() - lastBeatTimestamp;
	int referenceDuration = MINIMUM_DELAY_BETWEEN_BEATS - SINGLE_BEAT_DURATION;
	// float recencyFactor = constrain(1 - (float(referenceDuration) / durationSinceLastBeat), 0.0, 1.0);
	float recencyFactor = constrain(float(durationSinceLastBeat) / float(referenceDuration), 0.0, 1.0);
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

	int time = millis();

	// Update the EMA
	if (time < 5000) {
		heuristic_ema = alpha_short * heuristic + (1.0 - alpha_short) * heuristic_ema;
	}
	else {
		// if (!bufferInitialized) {
		// 	// Populate the buffer with the first 10 seconds of the heuristics
		// 	for (int i = 0; i < HEURISTIC_BUFFER_SIZE; i++) {
		// 		heuristicsBuffer[i] = heuristicsBuffer[i % heuristicsBufferIndex];
		// 	}
		// 	bufferInitialized = true;
		// }
		heuristic_ema = alpha_long * heuristic + (1.0 - alpha_long) * heuristic_ema;
	}
	heuristic /= heuristic_ema;

	// If the heuristic is above some percentile of the buffer, we have a beat
	float fudgeFactor = 1.05;
	float percentile = (1.0 - fudgeFactor * (1. / 30. * MAXIMUM_BEATS_PER_MINUTE / 60.)) * 100;


	float heuristicThreshold;
	float lowHeuristicsThreshold;
	if (time < 60000) {
		heuristicThreshold = 2.0;
		lowHeuristicsThreshold = 0.5;
	} else {
		heuristicThreshold = computePercentile(heuristicsBuffer, percentile);
		lowHeuristicsThreshold = computePercentile(heuristicsBuffer, 45);
	}

	// float heuristicThreshold = 1.5 * heuristic_ema;
	if (heuristic > heuristicThreshold) {
		lastBeatTimestamp = millis();
		Serial.print("BEAT (threshold) ");
		Serial.println(heuristicThreshold);
		elapsedBeats++;
	}

	Serial.print(heuristic);


	heuristicsBuffer[heuristicsBufferIndex] = heuristic;
	heuristicsBufferIndex = (heuristicsBufferIndex + 1) % HEURISTIC_BUFFER_SIZE;


	// // Clip this without storing it in the buffer
	// if (heuristic < heuristic_ema) {
	// 	heuristic = 0.0;
	// }

	return heuristic;
}