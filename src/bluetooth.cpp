#include <Arduino.h>
#include <BLEDevice.h>
#include <string>
#include "servos.hpp"
#include "shaders.hpp"

// UUIDs for BLE service and characteristic, randomly generated hex strings
#define SERVICE_UUID        		"4fafc204-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID 		"beb5483f-36e1-4688-b7f5-ea07361b26a8"
#define NOTIFY_CHARACTERISTIC_UUID 	"39f10f91-c4ee-d0fc-6dec-cbc5cfff5a9b"

extern ShaderManager shaderManager;
extern ServoManager servoManager;

extern int brightness;

String receivedValue = "None";

BLEServer* pServer;	
BLEService* pService;
BLECharacteristic* pCharacteristic;
BLECharacteristic* pNotifyCharacteristic;

void sendStringToPhone(String cmd, String data) {
    // Assuming pServer is a global or accessible variable
    if (!pServer->getConnectedCount()) {
        Serial.println("No devices connected.");
        return;
    }
	String s = cmd + ":" + data;
	Serial.println(s);
    pNotifyCharacteristic->setValue(s.c_str());
    pNotifyCharacteristic->notify();
}

class MyCallbacks : public BLECharacteristicCallbacks {

	void onWrite(BLECharacteristic* pCharacteristic) {
		std::string value = pCharacteristic->getValue();

		if (value == "getShaders") {
			String shaderNames = "";
			for (const auto& shader : shaderManager.shaders) {
				shaderNames += shader.first + ";"; // Use semicolon as a delimiter
			}
			sendStringToPhone("shaders", shaderNames);  // Send the list when commanded
		} 
		else if (value == "getIsAnimationActive") {
			sendStringToPhone("isAnimationActive", shaderManager.useAnimation ? "true" : "false");
		}
		else if (value == "getActiveShader") {
			sendStringToPhone("activeShader", shaderManager.activeShader->getName());
		}
		else if (value == "getAccentShaders") {
			String shaderNames = "";
			for (const auto& shader : shaderManager.accentShaders) {
				shaderNames += shader.first + ";"; // Use semicolon as a delimiter
			}
			sendStringToPhone("accentShaders", shaderNames);  // Send the list when commanded
		} 
		else if (value == "getActiveAccentShader") {
			sendStringToPhone("activeAccentShader", shaderManager.activeAccentShader->getName());
		}
		else if (value == "getServoSpeeds") {
			sendStringToPhone("servoSpeeds", servoManager.getServoSpeeds());
		} 
		else if (value == "getBrightness") {
			sendStringToPhone("brightness", String(brightness));
		}
		else if (value == "activateAnimation") {
			shaderManager.useAnimation = true;
		}
		else if (value == "deactivateAnimation") {
			shaderManager.useAnimation = false;
		}
		else {
			size_t pos = value.find(':');
			std::string cmd = value.substr(0, pos);
			std::string arg = value.substr(pos + 1);

			receivedValue = value.c_str();

			Serial.println("Received Values: ");
			Serial.println(cmd.c_str());
			Serial.println(arg.c_str());

			if (cmd == "setActiveShader") {
				shaderManager.setActiveShader(arg.c_str());
			} 
			else if (cmd == "setActiveAccentShader") {
				shaderManager.setActiveAccentShader(arg.c_str());
			} 
			else if (cmd == "setServoSpeed") {
				// Assume the value is formatted like "servo1;90"
				int pos = arg.find(";");
				if (pos != std::string::npos) {
					// std::string servo = arg.substr(0, pos);
					int servo = std::stoi(arg.substr(0, pos));
					float speed = std::stof(arg.substr(pos + 1));
					servoManager.setServoSpeed(servo, speed);
				}
			} 
			else if (cmd == "setBrightness") {
				// Assume the value is formatted like "servo1;90"
				int newBrightness = std::stoi(arg);
				brightness = newBrightness;
			}
			else {
				Serial.println("Invalid command");
			}
		}
	}
};

class MyServerCallbacks: public BLEServerCallbacks {

	bool hasEverConnected = false;


    void onConnect(BLEServer* server) {
        Serial.println("Device connected");
		hasEverConnected = true;
		shaderManager.hasPhoneEverConnected = true;
		servoManager.hasPhoneEverConnected = true;
    }

    void onDisconnect(BLEServer* server) {
        Serial.println("Device disconnected");
        // Start advertising again to allow a new connection
        BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
        pAdvertising->start();
        Serial.println("Advertising started");
    }
};

void bluetoothTask(void* args) {
	// Initialize BLE, called by setup() in main.cpp
	BLEDevice::init("Gyroled Totem");

    pServer = BLEDevice::createServer();
	pServer->setCallbacks(new MyServerCallbacks()); // Set the custom callbacks

    pService = pServer->createService(SERVICE_UUID);

    // Characteristic for receiving shader changes
    pCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE
    );

    // Characteristic for notifying shader list
    pNotifyCharacteristic = pService->createCharacteristic(
        NOTIFY_CHARACTERISTIC_UUID, // A new UUID for the notifying characteristic
        BLECharacteristic::PROPERTY_NOTIFY
    );

    pCharacteristic->setCallbacks(new MyCallbacks()); // Set the callback for write operations
    pCharacteristic->setValue("Hello World");
    pService->start();

	// Start advertising
	BLEAdvertising* pAdvertising = pServer->getAdvertising();
	pAdvertising->addServiceUUID(SERVICE_UUID);
	pAdvertising->setScanResponse(true);
	pAdvertising->setMinPreferred(0x06);  // Functions that help with faster connections
	pAdvertising->setMinPreferred(0x12);
	pAdvertising->start();
	Serial.println("Waiting for a client connection to notify...");

	// Task loop
	while (1) {
		// yield to other tasks
		vTaskDelay(500 / portTICK_PERIOD_MS);
	}
}

void setupBluetooth() {
	// Create a task that will handle initializing and running the BLE functionality
	#if MASTER_CONTROLLER
		int core = 1;  // async sampling gets run by core 0 so we put core 1 for less congestion (check if this actually helps)
	#else
		int core = CONFIG_ESP32_WIFI_TASK_PINNED_TO_CORE_0 ? 0 : 1;
	#endif

	TaskHandle_t bluetoothTaskHandle;
	xTaskCreatePinnedToCore(
		bluetoothTask,          // Task function
		"BluetoothTask",        // Name of the task
		10000,                  // Stack size in words
		NULL,                   // Task input parameter
		1,                      // Priority of the task
		&bluetoothTaskHandle,   // Task handle
		core                    // Core number
	);
}

