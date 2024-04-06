#include <Arduino.h>
#include <MD_MSGEQ7.h> 

// hardware pin definitions - change to suit circuit
#define DATA_PIN    13
#define RESET_PIN   33
#define STROBE_PIN  12
#define MAX_BAND 7 

// frequency reading the IC data
#define READ_DELAY  50

// LED Settings
#define LED_COUNT 120
#define LED_PIN   12

// unsigned long lastReadTime = 0;
// float updatesPerSecond = 0.0;
// unsigned long lastUpdate = 0;

// uint16_t bandValues[MAX_BAND] = {};


MD_MSGEQ7 MSGEQ7(RESET_PIN, STROBE_PIN, DATA_PIN);

void recordSpectrogramMSGEQ7(uint16_t bandValues[MAX_BAND]) {
    MSGEQ7.read();
    for (uint8_t i=0; i<MAX_BAND; i++) {
        bandValues[i] = MSGEQ7.get(i) / 10;
    }
}

void recordSpectrogramMSGEQ7_v2(uint16_t bandValues[MAX_BAND]) {
    // Read the MSGEQ7 IC
    digitalWrite(RESET_PIN, HIGH);
    digitalWrite(RESET_PIN, LOW);
    for (int i = 0; i < MAX_BAND; i++) {
        digitalWrite(STROBE_PIN, LOW);
        delayMicroseconds(30); // to allow the output to settle
        // The read value is 10-bit (0 to 1024).  PWM needs a value from 0 to 255, so divide by 4
        bandValues[i] = analogRead(DATA_PIN) >> 2;
        digitalWrite(STROBE_PIN, HIGH);
    }
    delay(READ_DELAY);




//   int frequencyBandVolume;
  
//   // Toggle the RESET pin of the MSGEQ7 to start reading from the lowest frequency band
//   digitalWrite(MSGEQ7_RESET_PIN, HIGH);
//   digitalWrite(MSGEQ7_RESET_PIN, LOW);
  
//   // Read the volume in every frequency band from the MSGEQ7
//   for (int i=0; i<NUM_FREQUENCY_BANDS; i++) {
//     digitalWrite(MSGEQ7_STROBE_PIN, LOW);
//     delayMicroseconds(30); // Allow the output to settle
//     frequencyBandVolume = analogRead(MSGEQ7_ANALOG_PIN);
//     digitalWrite(MSGEQ7_STROBE_PIN, HIGH);
    
//     // The read value is 10-bit (0 to 1024).  PWM needs a value from 0 to 255, so divide by 4
//     frequencyBandVolume = frequencyBandVolume >> 2;
    
//     // Fade the current LED value for this band
//     ledPWMValue[i] = ledPWMValue[i] > 7? ledPWMValue[i] - 7 : 0;
    
//     // Don't show the lower values
//     if (frequencyBandVolume > 70) {
//       // If the new volume is greater than that currently being showed then show the higher volume
//       if (frequencyBandVolume > ledPWMValue[i])
//         ledPWMValue[i] = frequencyBandVolume;
//     }
    
//     // Set the LED PWM value to the frequency band's volume
//     analogWrite(led[i],  ledPWMValue[i]);
//   }
  
//   // Wait before executing this loop again
//   delay(10);

}

void printSpectrogramMSGEQ7(uint16_t bandValues[MAX_BAND]) {
    String output;
    output += "Spectrogram: \n";
    for (int band = 0; band < MAX_BAND; band++) { 
        output += band;
        for(int i = 0; i < bandValues[band]; i++) {
            output += ("=");
        }
        output += "\n";
    }
    output += "\n\n";
    Serial.println(output);
}

void setup_MSGEQ7() {
    MSGEQ7.begin();
    analogReadResolution(10);
}

void setup_MSGEQ7_v2() {
    // Set up the MSGEQ7 IC
    pinMode(DATA_PIN, INPUT);
    pinMode(STROBE_PIN, OUTPUT);
    pinMode(RESET_PIN, OUTPUT);
    digitalWrite(RESET_PIN, LOW);
    digitalWrite(STROBE_PIN, HIGH);
}



/*

void setup() {
    Serial.begin(9600);
    Serial.println("[MD_MSG_SEQ7_Serial]");
    MSGEQ7.begin();
    analogReadResolution(10);
}

void loop() {

    updatesPerSecond = 1000.0 / float(millis() - lastUpdate);
    lastUpdate = millis();

    if(millis() > lastReadTime + READ_DELAY) {

        lastReadTime = millis();

        MSGEQ7.read();

        // Serial output
        for (uint8_t i=0; i<MAX_BAND; i++) {
            bandValues[i] = MSGEQ7.get(i);
            Serial.print(bandValues[i]);
            Serial.print('\t');
        }
        Serial.println();
        
    }

    printSpectrogram();

}

*/