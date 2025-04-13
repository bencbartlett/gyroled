#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <string.h>
#include "state.hpp"

// Hardcoded list of device MAC addresses (index 0: master; indexes 1-6: rings)
#define NUM_DEVICES 7
uint8_t BROADCAST_ALL[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
uint8_t deviceList[NUM_DEVICES][6] = {  // TODO: finish filling out this list
  {0xD8, 0x3B, 0xDA, 0x45, 0xCE, 0x4C},  // master
  {0x24, 0x58, 0x7C, 0xE4, 0x1A, 0x50},  // ring 1 (outer ring)
  {0x98, 0x3D, 0xAE, 0xEA, 0x0D, 0xC8},  // ring 2
  {0x98, 0x3D, 0xAE, 0xE5, 0xEF, 0xF4},  // ring 3
  {0x98, 0x3D, 0xAE, 0xEA, 0x23, 0x74},  // ring 4
  {0x24, 0xEC, 0x4A, 0x00, 0x4E, 0xC0},  // ring 5
  {0x98, 0x3D, 0xAE, 0xE7, 0x92, 0x50}   // ring 6 (sphere)
};
unsigned long heartbeats[NUM_DEVICES] = {0};


enum DeviceRole { MASTER, RING };

extern ServoController servoController;

extern State state;

static volatile bool tx_done      = true;
static volatile esp_now_send_status_t last_status;

void IRAM_ATTR onDataSent(const uint8_t *mac,
                          esp_now_send_status_t status)
{
    last_status = status;
    tx_done     = true;            // free the “slot”
}

void sendStateToRing(const uint8_t *addr, const State &st)
{
    while (!tx_done) {             // wait for previous packet to drain
        vTaskDelay(1);             // give Wi‑Fi task time (~1 ms)
    }

    tx_done = false;
    esp_err_t err = esp_now_send(addr,
                                 reinterpret_cast<const uint8_t*>(&st),
                                 sizeof(State));
    if (err != ESP_OK) {
        Serial.printf("[ERR] send: %s\n", esp_err_to_name(err));
        tx_done = true;            // don’t dead‑lock on failure
    }
}

class Synchronizer {

public:

	DeviceRole role;
	int deviceIndex; // 0 = master, 1-6 = ring index

	float current_angles[6] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

	Synchronizer() {

	}

	void init() {
		Serial.begin(115200);
		WiFi.mode(WIFI_STA);

		// Determine device role based on MAC address.
		uint8_t ownMac[6];
		WiFi.macAddress(ownMac);
		Serial.print("Device MAC Address: ");
		for (int i = 0; i < 6; i++) {
			Serial.print("0x");
			Serial.print(ownMac[i], HEX);
			if (i < 5) Serial.print(", ");
		}
		Serial.println();
		bool recognized = false;
		for (int i = 0; i < NUM_DEVICES; i++) {
			if (memcmp(ownMac, deviceList[i], 6) == 0) {
				recognized = true;
				deviceIndex = i;
				break;
			}
		}
		if (!recognized) {
			Serial.println("Device MAC not recognized. Defaulting to ring role at index 1.");
			role = RING;
			deviceIndex = 1;
		}
		else {
			role = (deviceIndex == 0) ? MASTER : RING;
		}
		Serial.print("Role: ");
		Serial.println(role == MASTER ? "MASTER" : "RING");
		Serial.print("Device index: ");
		Serial.println(deviceIndex);

		// Initialize ESP-NOW.
		if (esp_now_init() != ESP_OK) {
			Serial.println("Error initializing ESP-NOW");
			while (true);
		}
		esp_now_register_recv_cb(onDataRecv);

		if (role == MASTER) {
			esp_now_register_send_cb(onDataSent);   // register TX‑done callback
		}

		// Add peers.
		if (role == MASTER) {
			delay(1000); // Wait for ESP-NOW to stabilize on rings
			// Master adds all rings.
			for (int i = 1; i < NUM_DEVICES; i++) {
				esp_now_peer_info_t peerInfo = {};
				memcpy(peerInfo.peer_addr, deviceList[i], 6);
				peerInfo.channel = 0;
				peerInfo.encrypt = false;
				if (esp_now_add_peer(&peerInfo) != ESP_OK) {
					Serial.print("Failed to add ring peer ");
					Serial.println(i);
				}
			}
			state.isPaused = false;
		}
		else {
			// ring adds master.
			esp_now_peer_info_t peerInfo = {};
			memcpy(peerInfo.peer_addr, deviceList[0], 6);
			peerInfo.channel = 0;
			peerInfo.encrypt = false;
			if (esp_now_add_peer(&peerInfo) != ESP_OK) {
				Serial.println("Failed to add master peer");
			}
		}

		servoController.ringIndex = deviceIndex;   // pass index to servo layer
	}

	// Static pointer for use in the static callback.
	static Synchronizer* instance;

	// Static callback wrapper.
	static void onDataRecv(const uint8_t* mac, const uint8_t* incomingData, int len) {
		if (instance) {
			// This bridges the gap between the static C style callback required by ESP-now and the instance method
			instance->handleReceive(mac, incomingData, len);
		}
	}

	// Handle received data.
	void handleReceive(const uint8_t* mac, const uint8_t* incomingData, int len) {
		if (role == MASTER) {
			if (len == sizeof(float)) {
				float receivedAngle;
				memcpy(&receivedAngle, incomingData, sizeof(float));
				int senderIndex = -1;
				for (int i = 1; i < NUM_DEVICES; i++) {
					if (memcmp(mac, deviceList[i], 6) == 0) {
						senderIndex = i;
						break;
					}
				}
				if (senderIndex != -1) {
					current_angles[senderIndex - 1] = receivedAngle;
					heartbeats[senderIndex] = millis(); // heard from ring
				} else {
					Serial.println("Received data from unknown ring.");
				}
			}
		}
		else {
			if (len == sizeof(State)) {
				// Copy master‑sent state directly into the global state object
				memcpy(&state, incomingData, sizeof(State));

				servoController.lastStateReceived = millis();
				heartbeats[0] = millis();                 // heard from master

				// Pick out the new target angle for *this* ring
				switch (deviceIndex) {
					case 1: 
						servoController.target_angle = state.target_angle_1;
						servoController.target_angular_velocity = state.target_angular_velocity_1;
						break;
					case 2:
						servoController.target_angle = state.target_angle_2;
						servoController.target_angular_velocity = state.target_angular_velocity_2;
						break;
					case 3:
						servoController.target_angle = state.target_angle_3;
						servoController.target_angular_velocity = state.target_angular_velocity_3;
						break;
					case 4:
						servoController.target_angle = state.target_angle_4;
						servoController.target_angular_velocity = state.target_angular_velocity_4;
						break;
					case 5:
						servoController.target_angle = state.target_angle_5;
						servoController.target_angular_velocity = state.target_angular_velocity_5;
						break;
					case 6:
						servoController.target_angle = state.target_angle_6;
						servoController.target_angular_velocity = state.target_angular_velocity_6;
						break;
					default:
						Serial.println("Unknown device index.");
						return;
				}
			}
		}
	}

	// Send data: master sends full state; ring sends back its servo angle.
	void synchronize() {
		if (role == MASTER) {
			// Serial.println("Master sending state to all rings.");
			// esp_err_t result = esp_now_send(BROADCAST_ALL, (uint8_t*)&state, sizeof(State));
			// if (result != ESP_OK) {
			// 	Serial.print("Error sending state from master:");
			// 	Serial.println(esp_err_to_name(result));
			// } else {
			// 	// Print contents of state
			// 	state.print();
			// }

			for (int i = 1; i < NUM_DEVICES; ++i) {
				sendStateToRing(deviceList[i], state);
			}

			// for (int i = 1; i < NUM_DEVICES; i++) {
			// 	delay(10);
			// 	esp_err_t result = esp_now_send(deviceList[i], reinterpret_cast<uint8_t*>(&state), sizeof(State));
			// 	if (result == ESP_OK) {
			// 		Serial.print("Sent state to ring ");
			// 		Serial.println(i);
			// 	}
			// 	else {
			// 		Serial.print("Error sending to ring ");
			// 		Serial.print(i);
			// 		Serial.print(": ");
			// 		Serial.println(esp_err_to_name(result));
			// 	}
			// }
			state.print();

		}
		else {
			esp_err_t result = esp_now_send(deviceList[0], (uint8_t*)&servoController.current_angle, sizeof(float));
			if (result != ESP_OK) {
				Serial.println("Error sending angle.");
			}
			state.print();
		}
	}
};

Synchronizer* Synchronizer::instance = nullptr;
