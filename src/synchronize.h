#ifndef SYNCHRONIZER_H
#define SYNCHRONIZER_H

#include <WiFi.h>
#include <esp_now.h>
#include <string.h>

#define NUM_DEVICES 7

// Hardcoded list of device MAC addresses (index 0: master; indexes 1-6: slaves)
// Defined in the corresponding .cpp file.
extern uint8_t deviceList[NUM_DEVICES][6];

enum DeviceRole { MASTER, SLAVE };

// Struct representing the complete state transmitted from master to slaves.
struct State {
    int shader_index;
    float beat_intensity;
    float angle_1;
    float angle_2;
    float angle_3;
    float angle_4;
    float angle_5;
    float angle_6;
};

class Synchronizer {
public:
    State state;
    DeviceRole role;
    int deviceIndex; // 0 = master; 1-6 = slave indices

    Synchronizer();

    // Initialize WiFi, ESP-NOW, and determine device role.
    void init();

    // Send data: master sends full state; slave sends back its servo angle.
    void send();

    // Dummy function to get current servo angle (to be replaced with actual code).
    float getServoAngle();

    // Handle incoming data based on device role.
    void handleReceive(const uint8_t* mac, const uint8_t* incomingData, int len);

    // Static callback for ESP-NOW receive.
    static void onDataRecv(const uint8_t* mac, const uint8_t* incomingData, int len);

    // Static instance pointer for use in the callback.
    static Synchronizer* instance;
};

// Global instance declaration (defined in the .cpp file).
extern Synchronizer synchronizer;

#endif // SYNCHRONIZER_H