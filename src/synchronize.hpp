#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <string.h>

// Hardcoded list of device MAC addresses (index 0: master; indexes 1-6: rings)
//#define NUM_DEVICES 7
#define NUM_DEVICES 2
uint8_t deviceList[NUM_DEVICES][6] = {  // TODO: finish filling out this list
  {0xD8, 0x3B, 0xDA, 0x45, 0xCE, 0x4C},  // master
//   {0x24, 0x6F, 0x28, 0xDD, 0xEE, 0xFF}   // ring 1 (only ring)
  {0x98, 0x3D, 0xAE, 0xE5, 0xEF, 0xF4},  // ring 2
  // {0x24, 0x6F, 0x28, 0x44, 0x55, 0x66},  // ring 3
  // {0x24, 0x6F, 0x28, 0x77, 0x88, 0x99},  // ring 4
  // {0x24, 0x6F, 0x28, 0xAA, 0xBB, 0x01},  // ring 5
  // {0x24, 0x6F, 0x28, 0xCC, 0xDD, 0x02}   // ring 6 (sphere)
};

enum DeviceRole { MASTER, RING };

extern ServoController servoController;

// The synchronization packet sent over ESP‑NOW
struct State {

	bool isPaused = true;

    // Frame/tempo bookkeeping
    uint16_t frame            = 0;
    unsigned long time        = 0;
	unsigned long lastUpdate  = 0;
    float updatesPerSecond    = 50.0f; 

    // Visual state
    int   shader_index   = 0;
    float beat_intensity = 0.0f;

    // Per‑ring telemetry
    float current_angle_1 = 0.0f;
    float target_angle_1  = 0.0f;

    float current_angle_2 = 0.0f;
    float target_angle_2  = 0.0f;

    float current_angle_3 = 0.0f;
    float target_angle_3  = 0.0f;

    float current_angle_4 = 0.0f;
    float target_angle_4  = 0.0f;

    float current_angle_5 = 0.0f;
    float target_angle_5  = 0.0f;

    float current_angle_6 = 0.0f;
    float target_angle_6  = 0.0f;
};

extern State state;

class Synchronizer {

public:

	DeviceRole role;
	int deviceIndex; // 0 = master, 1-6 = ring index

	Synchronizer() {
		state.time			 = 0;
		state.lastUpdate		 = 0;
		state.updatesPerSecond = 50.0f;
		state.frame			 = 0;

		state.shader_index      = 0;
		state.beat_intensity     = 0.0f;

		state.current_angle_1    = 0.0f;
		state.target_angle_1     = 90.0f;

		state.current_angle_2    = 0.0f;
		state.target_angle_2     = 90.0f;

		state.current_angle_3    = 0.0f;
		state.target_angle_3     = 90.0f;

		state.current_angle_4    = 0.0f;
		state.target_angle_4     = 90.0f;

		state.current_angle_5    = 0.0f;
		state.target_angle_5     = 90.0f;

		state.current_angle_6    = 0.0f;
		state.target_angle_6     = 90.0f;
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
					case 1: state.current_angle_1 = receivedAngle; break;
					case 2: state.current_angle_2 = receivedAngle; break;
					case 3: state.current_angle_3 = receivedAngle; break;
					case 4: state.current_angle_4 = receivedAngle; break;
					case 5: state.current_angle_5 = receivedAngle; break;
					case 6: state.current_angle_6 = receivedAngle; break;
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
			if (len == sizeof(State)) {
				State masterState;
				memcpy(&masterState, incomingData, sizeof(State));
				float targetAngle = 0;
				switch (deviceIndex) {
				case 1: targetAngle = masterState.target_angle_1; break;
				case 2: targetAngle = masterState.target_angle_2; break;
				case 3: targetAngle = masterState.target_angle_3; break;
				case 4: targetAngle = masterState.target_angle_4; break;
				case 5: targetAngle = masterState.target_angle_5; break;
				case 6: targetAngle = masterState.target_angle_6; break;
				}
				servoController.target_angle = targetAngle;
				Serial.print("Ring ");
				Serial.print(deviceIndex);
				Serial.print(" received target angle: ");
				Serial.println(targetAngle);
			}
		}
	}

	// Send data: master sends full state; ring sends back its servo angle.
	void synchronize() {
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
			Serial.printf("Shader: %d | Beat: %.2f\n", state.shader_index, state.beat_intensity);
			for (int i = 1; i <= 6; ++i) {
				float cur = 0, tgt = 0;
				switch (i) {
					case 1: cur = state.current_angle_1; tgt = state.target_angle_1; break;
					case 2: cur = state.current_angle_2; tgt = state.target_angle_2; break;
					case 3: cur = state.current_angle_3; tgt = state.target_angle_3; break;
					case 4: cur = state.current_angle_4; tgt = state.target_angle_4; break;
					case 5: cur = state.current_angle_5; tgt = state.target_angle_5; break;
					case 6: cur = state.current_angle_6; tgt = state.target_angle_6; break;
				}
				Serial.printf("Ring %d  cur: %.1f°  tgt: %.1f°\n", i, cur, tgt);
			}

		}
		else {
			float currentAngle = servoController.current_angle;
			Serial.print("[DEBUG] Ring ");
			Serial.print(deviceIndex);
			Serial.print(" sending current angle (deg): ");
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
