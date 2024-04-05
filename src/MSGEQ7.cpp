#include <MD_MSGEQ7.h> 

// hardware pin definitions - change to suit circuit
#define DATA_PIN    13
#define RESET_PIN   33
#define STROBE_PIN  12

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