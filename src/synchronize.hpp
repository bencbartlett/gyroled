#include <WiFi.h>
#include <esp_now.h>
#include <string.h>

// Hardcoded list of device MAC addresses (index 0: master; indexes 1-6: rings)
//#define NUM_DEVICES 7
#define NUM_DEVICES 2
uint8_t deviceList[NUM_DEVICES][6] = {
  {0xD8, 0x3B, 0xDA, 0x45, 0xCE, 0x4C},  // master
//   {0x24, 0x6F, 0x28, 0xDD, 0xEE, 0xFF}   // ring 1 (only ring)
  {0x98, 0x3D, 0xAE, 0xE5, 0xEF, 0xF4},  // ring 2
  // {0x24, 0x6F, 0x28, 0x44, 0x55, 0x66},  // ring 3
  // {0x24, 0x6F, 0x28, 0x77, 0x88, 0x99},  // ring 4
  // {0x24, 0x6F, 0x28, 0xAA, 0xBB, 0x01},  // ring 5
  // {0x24, 0x6F, 0x28, 0xCC, 0xDD, 0x02}   // ring 6 (sphere)
};

enum DeviceRole { MASTER, RING };

// The state struct transmitted from master to rings.
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
	int deviceIndex; // 0 = master, 1-6 = ring index

	Synchronizer() {
		// Initialize default state values.
		state.shader_index = 0;
		state.beat_intensity = 0.0;
		state.angle_1 = 90;
		state.angle_2 = 90;
		state.angle_3 = 90;
		state.angle_4 = 90;
		state.angle_5 = 90;
		state.angle_6 = 90;
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
			// Expect a float from a ring.
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
					switch (senderIndex) {
					case 1: state.angle_1 = receivedAngle; break;
					// case 2: state.angle_2 = receivedAngle; break;
					// case 3: state.angle_3 = receivedAngle; break;
					// case 4: state.angle_4 = receivedAngle; break;
					// case 5: state.angle_5 = receivedAngle; break;
					// case 6: state.angle_6 = receivedAngle; break;
					}
					Serial.print("Master updated angle from ring ");
					Serial.print(senderIndex);
					Serial.print(": ");
					Serial.println(receivedAngle);
				}
				else {
					Serial.println("Received data from unknown ring.");
				}
			}
		}
		else {
			// Ring expects a full State struct from master.
			if (len == sizeof(State)) {
				State masterState;
				memcpy(&masterState, incomingData, sizeof(State));
				float targetAngle = 0;
				switch (deviceIndex) {
				case 1: targetAngle = masterState.angle_1; break;
				// case 2: targetAngle = masterState.angle_2; break;
				// case 3: targetAngle = masterState.angle_3; break;
				// case 4: targetAngle = masterState.angle_4; break;
				// case 5: targetAngle = masterState.angle_5; break;
				// case 6: targetAngle = masterState.angle_6; break;
				}
				Serial.print("Ring ");
				Serial.print(deviceIndex);
				Serial.print(" received target angle: ");
				Serial.println(targetAngle);
				// Here, update the local servo if needed.
			}
		}
	}

	// Send data: master sends full state; ring sends back its servo angle.
	void send() {
		if (role == MASTER) {
			Serial.println("Master sending state to all rings.");
			for (int i = 1; i < NUM_DEVICES; i++) {
				esp_err_t result = esp_now_send(deviceList[i], (uint8_t*)&state, sizeof(State));
				if (result == ESP_OK) {
					Serial.print("Sent state to ring ");
					Serial.println(i);
				}
				else {
					Serial.print("Error sending to ring ");
					Serial.println(i);
				}
			}

			Serial.println("\n[DEBUG] Master sending state:");
			Serial.print("Shader Index: "); Serial.println(state.shader_index);
			Serial.print("Beat Intensity: "); Serial.println(state.beat_intensity);
			Serial.print("Angle 1: "); Serial.println(state.angle_1);
			Serial.print("Angle 2: "); Serial.println(state.angle_2);
			Serial.print("Angle 3: "); Serial.println(state.angle_3);
			Serial.print("Angle 4: "); Serial.println(state.angle_4);
			Serial.print("Angle 5: "); Serial.println(state.angle_5);
			Serial.print("Angle 6: "); Serial.println(state.angle_6);

		}
		else {
			float currentAngle = millis() / 1000.0;
			Serial.print("[DEBUG] Ring ");
			Serial.print(deviceIndex);
			Serial.print(" sending current angle (seconds since start): ");
			Serial.println(currentAngle);
			esp_err_t result = esp_now_send(deviceList[0], (uint8_t*)&currentAngle, sizeof(float));
			if (result == ESP_OK) {
				Serial.println("Sent angle to master.");
			}
			else {
				Serial.println("Error sending angle.");
			}
		}
	}

};

Synchronizer* Synchronizer::instance = nullptr;
