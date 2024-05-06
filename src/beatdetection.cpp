// #include <Arduino.h>

// long lastBeatTimestamp = 0;
// long durationSinceLastBeat = 0;
// float beatProbability = 0;
// float beatProbabilityThreshold = 0.5;



// /**
//  * Will update the beat probability, a value between 0 and 1
//  * indicating how likely it is that there's a beat right now.
//  */
// void updateBeatProbability() {
//   beatProbability = 1;
//   beatProbability *= calculateSignalChangeFactor();
//   beatProbability *= calculateMagnitudeChangeFactor();
//   beatProbability *= calculateVarianceFactor();
//   beatProbability *= calculateRecencyFactor();
  
//   if (beatProbability >= beatProbabilityThreshold) {
//     lastBeatTimestamp = millis();
//     durationSinceLastBeat = 0;
//   }
  
// //   logValue("B", beatProbability, 5);
// }

// /**
//  * Will calculate a value in range [0:2] based on the magnitude changes of
//  * different frequency bands.
//  * Low values are indicating a low beat probability.
//  */
// float calculateSignalChangeFactor() {
//   float aboveAverageSignalFactor;
//   if (averageSignal < 75 || currentSignal < 150) {
//     aboveAverageSignalFactor = 0;
//   } else {
//     aboveAverageSignalFactor = ((float) currentSignal / averageSignal);
//     aboveAverageSignalFactor = constrain(aboveAverageSignalFactor, 0, 2);
//   }
//   return aboveAverageSignalFactor;
// }

// /**
//  * Will calculate a value in range [0:1] based on the magnitude changes of
//  * different frequency bands.
//  * Low values are indicating a low beat probability.
//  */
// float calculateMagnitudeChangeFactor() {
//   float changeThresholdFactor = 1.1;
//   if (durationSinceLastBeat < 750) {
//     // attempt to not miss consecutive beats
//     changeThresholdFactor *= 0.95;
//   } else if (durationSinceLastBeat > 1000) {
//     // reduce false-positives
//     changeThresholdFactor *= 1.05;
//   }
  
//   // current overall magnitude is higher than the average, probably 
//   // because the signal is mainly noise
//   float aboveAverageOverallMagnitudeFactor = ((float) currentOverallFrequencyMagnitude / averageOverallFrequencyMagnitude);
//   aboveAverageOverallMagnitudeFactor -= 1.05;
//   aboveAverageOverallMagnitudeFactor *= 10;
//   aboveAverageOverallMagnitudeFactor = constrain(aboveAverageOverallMagnitudeFactor, 0, 1);
  
//   // current magnitude is higher than the average, probably 
//   // because the there's a beat right now
//   float aboveAverageFirstMagnitudeFactor = ((float) currentFirstFrequencyMagnitude / averageFirstFrequencyMagnitude);
//   aboveAverageOverallMagnitudeFactor -= 0.1;
//   aboveAverageFirstMagnitudeFactor *= 1.5;
//   aboveAverageFirstMagnitudeFactor = pow(aboveAverageFirstMagnitudeFactor, 3);
//   aboveAverageFirstMagnitudeFactor /= 3;
//   aboveAverageFirstMagnitudeFactor -= 1.25;
  
//   aboveAverageFirstMagnitudeFactor = constrain(aboveAverageFirstMagnitudeFactor, 0, 1);
  
//   float aboveAverageSecondMagnitudeFactor = ((float) currentSecondFrequencyMagnitude / averageSecondFrequencyMagnitude);
//   aboveAverageSecondMagnitudeFactor -= 1.01;
//   aboveAverageSecondMagnitudeFactor *= 10;
//   aboveAverageSecondMagnitudeFactor = constrain(aboveAverageSecondMagnitudeFactor, 0, 1);
  
//   float magnitudeChangeFactor = aboveAverageFirstMagnitudeFactor;
//   if (magnitudeChangeFactor > 0.15) {
//     magnitudeChangeFactor = max(aboveAverageFirstMagnitudeFactor, aboveAverageSecondMagnitudeFactor);
//   }
  
//   if (magnitudeChangeFactor < 0.5 && aboveAverageOverallMagnitudeFactor > 0.5) {
//     // there's no bass related beat, but the overall magnitude changed significantly
//     magnitudeChangeFactor = max(magnitudeChangeFactor, aboveAverageOverallMagnitudeFactor);
//   } else {
//     // this is here to avoid treating signal noise as beats
//     //magnitudeChangeFactor *= 1 - aboveAverageOverallMagnitudeFactor;
//   }
  
//   return magnitudeChangeFactor;
// }

// /**
//  * Will calculate a value in range [0:1] based on variance in the first and second
//  * frequency band over time. The variance will be high if the magnitude of bass
//  * frequencies changed in the last few milliseconds.
//  * Low values are indicating a low beat probability.
//  */
// float calculateVarianceFactor() {
//   // a beat also requires a high variance in recent frequency magnitudes
//   float firstVarianceFactor = ((float) (firstFrequencyMagnitudeVariance - 50) / 20) - 1;
//   firstVarianceFactor = constrain(firstVarianceFactor, 0, 1);
  
//   float secondVarianceFactor = ((float) (secondFrequencyMagnitudeVariance - 50) / 20) - 1;
//   secondVarianceFactor = constrain(secondVarianceFactor, 0, 1);
  
//   float varianceFactor = max(firstVarianceFactor, secondVarianceFactor);
  
//   logValue("V", varianceFactor, 1);
  
//   return varianceFactor;
// }

// /**
//  * Will calculate a value in range [0:1] based on the recency of the last detected beat.
//  * Low values are indicating a low beat probability.
//  */
// float calculateRecencyFactor() {
//   float recencyFactor = 1;
//   durationSinceLastBeat = millis() - lastBeatTimestamp;
  
//   int referenceDuration = MINIMUM_DELAY_BETWEEN_BEATS - SINGLE_BEAT_DURATION;
//   recencyFactor = 1 - ((float) referenceDuration / durationSinceLastBeat);
//   recencyFactor = constrain(recencyFactor, 0, 1);
  
//   //logValue("R", recencyFactor, 5);
  
//   return recencyFactor;
// }



// void processHistoryValues(byte history[], int &historyIndex, int &current, int &total, int &average, int &variance) {
//   total -= history[historyIndex]; // subtract the oldest history value from the total
//   total += (byte) current; // add the current value to the total
//   history[historyIndex] = current; // add the current value to the history
  
//   average = total / FREQUENCY_MAGNITUDE_SAMPLES;
  
//   // update the variance of frequency magnitudes
//   long squaredDifferenceSum = 0;
//   for (int i = 0; i < FREQUENCY_MAGNITUDE_SAMPLES; i++) {
//     squaredDifferenceSum += pow(history[i] - average, 2);
//   }
//   variance = (double) squaredDifferenceSum / FREQUENCY_MAGNITUDE_SAMPLES;
// }


// void processFrequencyData() {
//   // each of the methods below will:
//   //  - get the current frequency magnitude
//   //  - add the current magnitude to the history
//   //  - update relevant features
//   processOverallFrequencyMagnitude();
//   processFirstFrequencyMagnitude();
//   processSecondFrequencyMagnitude();
  
//   // prepare the magnitude sample index for the next update
//   frequencyMagnitudeSampleIndex += 1;
//   if (frequencyMagnitudeSampleIndex >= FREQUENCY_MAGNITUDE_SAMPLES) {
//     frequencyMagnitudeSampleIndex = 0; // wrap the index
//   }
// }

// void processFirstFrequencyMagnitude() {
//   currentFirstFrequencyMagnitude = getFrequencyMagnitude(
//     fht_log_out, 
//     FIRST_FREQUENCY_RANGE_START, 
//     FIRST_FREQUENCY_RANGE_END
//   );
  
//   processHistoryValues(
//     firstFrequencyMagnitudes, 
//     frequencyMagnitudeSampleIndex, 
//     currentFirstFrequencyMagnitude, 
//     totalFirstFrequencyMagnitude, 
//     averageFirstFrequencyMagnitude, 
//     firstFrequencyMagnitudeVariance
//   );
// }

// void processSecondFrequencyMagnitude() {
//   currentSecondFrequencyMagnitude = getFrequencyMagnitude(
//     fht_log_out, 
//     SECOND_FREQUENCY_RANGE_START, 
//     SECOND_FREQUENCY_RANGE_END
//   );
  
//   processHistoryValues(
//     secondFrequencyMagnitudes, 
//     frequencyMagnitudeSampleIndex, 
//     currentSecondFrequencyMagnitude, 
//     totalSecondFrequencyMagnitude, 
//     averageSecondFrequencyMagnitude, 
//     secondFrequencyMagnitudeVariance
//   );
// }


