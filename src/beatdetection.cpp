#include <Arduino.h>
#include <algorithm>  // Include for std::nth_element and std::sort
#include <cmath>      // Include for std::floor and std::ceil

#include "fft.h"


#define HEURISTIC_BUFFER_SIZE 			2048  // at 30fps this is 68 seconds of context
#define MAXIMUM_BEATS_PER_MINUTE	 	155
#define TYPICAL_BEATS_PER_MINUTE 		128
#define SINGLE_BEAT_DURATION			100  // in ms, good value range is [50:150]
const int MINIMUM_DELAY_BETWEEN_BEATS = 60000L / MAXIMUM_BEATS_PER_MINUTE;
const int TYPICAL_DELAY_BETWEEN_BEATS = 60000L / TYPICAL_BEATS_PER_MINUTE;

float heuristicsBuffer[HEURISTIC_BUFFER_SIZE] = {};
uint16_t heuristicsBufferIndex = 0;
float bufferCopy[HEURISTIC_BUFFER_SIZE] = {};

float spectrumFiltered[SAMPLES / 2] = {};
float spectrumFilteredPrev[SAMPLES / 2] = {};

static float spectrogram[NUM_GEQ_CHANNELS] = {0. };

static float fftProcessed[NUM_GEQ_CHANNELS] = { 0 };// Our calculated freq. channel result table to be used by effects
static float fftProcessedPrev[NUM_GEQ_CHANNELS] = { 0 };// Our calculated freq. channel result table to be used by effects

const float alpha_short = 1.0 / (30.0 * 5.0); // roughly 5 seconds
const float alpha_long = 1.0 / (30.0 * 60.0); // roughly 60 seconds
float heuristic_ema = 1.0;
float heuristicThreshold = 2.5; // empirical starting value for until heuristicsBuffer is populated
bool bufferInitialized = false;

unsigned long lastBeatTimestamp = 0;
unsigned long elapsedBeats = 0;

float frequencies[SAMPLES / 2] = { 0. };

extern int frame; // from main.cpp
extern float updatesPerSecond; // from main.cpp


float calculateRecencyFactor() {
	unsigned long durationSinceLastBeat = millis() - lastBeatTimestamp;
	// int referenceDuration = MINIMUM_DELAY_BETWEEN_BEATS - SINGLE_BEAT_DURATION;
	int referenceDuration = TYPICAL_DELAY_BETWEEN_BEATS - SINGLE_BEAT_DURATION;
	float maxRecencyFactor = 1.2;
	// float recencyFactor = constrain(1 - (float(referenceDuration) / durationSinceLastBeat), 0.0, maxRecencyFactor);
	float recencyFactor = constrain(float(durationSinceLastBeat) / float(referenceDuration), 0.0, maxRecencyFactor);
	return recencyFactor;
}


float computePercentile(float buffer[HEURISTIC_BUFFER_SIZE], float percentile) {
	// Calculate the index corresponding to the percentile
	int index = static_cast<int>(std::ceil(percentile / 100.0 * HEURISTIC_BUFFER_SIZE) - 1);
	index = std::max(0, std::min(index, HEURISTIC_BUFFER_SIZE - 1));
	// Create a copy of the buffer to avoid modifying the original
	memcpy(bufferCopy, buffer, HEURISTIC_BUFFER_SIZE * sizeof(float(0)));
	// Use std::nth_element to rearrange elements
	std::nth_element(bufferCopy, bufferCopy + index, bufferCopy + HEURISTIC_BUFFER_SIZE);
	// Return the element at the index
	return bufferCopy[index];
}


float computeBeatHeuristic() {

	doFFT(frequencies);

	applyKickDrumIsolationFilter(frequencies, spectrumFiltered);  // applies to spectrumFiltered in-place
	computeSpectrogramWLED(spectrumFiltered, spectrogram); 

	// computeSpectrogramWLED(frequencies, spectrogram); 
	
	postProcessFFTResults(spectrogram, fftProcessed);

	float entropyChange = calculateEntropyChange(fftProcessed, fftProcessedPrev);
	float heuristic = entropyChange;

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

	// Write the heuristic to the buffer
	heuristicsBuffer[heuristicsBufferIndex] = heuristic;
	heuristicsBufferIndex = (heuristicsBufferIndex + 1) % HEURISTIC_BUFFER_SIZE;

	// PostProcessing
	float heuristicPostProcessed = heuristic;

	// If the heuristic is above some percentile of the buffer, we have a beat
	float fudgeFactor = 1.05;
	float secondsPerFrame = 1. / updatesPerSecond;
	float typicalBeatsPerSecond = TYPICAL_BEATS_PER_MINUTE / 60.;
	float typicalBeatsPerFrame = typicalBeatsPerSecond * secondsPerFrame;

	float percentile = (1.0 - fudgeFactor * typicalBeatsPerFrame) * 100.0;

	// float lowHeuristicsThreshold;
	if (frame % 30 == 0) {
		if (time > 30000) {
			heuristicThreshold = computePercentile(heuristicsBuffer, percentile);
			// lowHeuristicsThreshold = computePercentile(heuristicsBuffer, 45);
		}
	}

	const bool convolveWithPreviousBeats = true;
	const float previousBeatBonus = 0.25;
	// Convolve the heuristics with the previous beats, counting heuristics that are ~126 beats earlier
	// This could help to fix missing beats
	int expectedBpm = 126;
	int bpmWindow = 15;
	if (convolveWithPreviousBeats) {
		// Find the heuristics entries from the buffer that are within expectedBpm +/- bpmWindow
		float beatDurationMin = 60. / (expectedBpm + bpmWindow);  // number of seconds per beat
		float beatDurationMax = 60. / (expectedBpm - bpmWindow);
		int timestepsAgoMin = int(updatesPerSecond * beatDurationMin);
		int timestepsAgoMax = int(updatesPerSecond * beatDurationMax);

		// Sum the heuristics in the range
		float avgHeuristicInWindow = 0.0;
		float maxHeuristicInWindow = 0.0;
		for (int i = timestepsAgoMin; i <= timestepsAgoMax; i++) {
			int index = (heuristicsBufferIndex - i + HEURISTIC_BUFFER_SIZE) % HEURISTIC_BUFFER_SIZE;
			avgHeuristicInWindow += heuristicsBuffer[index];
			maxHeuristicInWindow = std::max(maxHeuristicInWindow, heuristicsBuffer[index]);
		}
		avgHeuristicInWindow /= (timestepsAgoMax - timestepsAgoMin + 1);

		if (avgHeuristicInWindow > heuristicThreshold) {
			heuristicPostProcessed += previousBeatBonus * maxHeuristicInWindow;
		}
	}

		// float heuristicThreshold = 1.5 * heuristic_ema;
	heuristicPostProcessed *= calculateRecencyFactor();
	if (heuristicPostProcessed > heuristicThreshold) {
		lastBeatTimestamp = millis();
		// Serial.print("BEAT (threshold) ");
		// Serial.println(heuristicThreshold);
		elapsedBeats++;
	}


	// float emaThreshold = 1.3;
	// float boostingFactor = 1.5;
	// if (heuristic > heuristic_ema * emaThreshold) {
	// 	heuristic += boostingFactor * (heuristic - heuristic_ema * emaThreshold);
	// } 
	// else if (heuristic < heuristic_ema / emaThreshold)  {
	// 	heuristic -= boostingFactor * (heuristic_ema / emaThreshold - heuristic);
	// }

	// Serial.print(heuristic);



	// // Clip this without storing it in the buffer
	// if (heuristic < heuristic_ema) {
	// 	heuristic = 0.0;
	// }

	// return heuristic;
	return heuristicPostProcessed;
}