#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <string>
// #include <sstream>
#include "servos.hpp"
#include "shaders.hpp"

// UUIDs for BLE service and characteristic
#define SERVICE_UUID        		"4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID 		"beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define NOTIFY_CHARACTERISTIC_UUID 	"39f10f90-c4ee-d0fc-6dec-cbc5cfff5a9b"

extern ShaderManager shaderManager;
extern ServoManager servoManager;


String receivedValue = "None";

BLEServer* pServer;
BLEService* pService;
BLECharacteristic* pCharacteristic;
BLECharacteristic* pNotifyCharacteristic;

void sendShaderList() {
    // Assuming pServer is a global or accessible variable
    if (!pServer->getConnectedCount()) {
        Serial.println("No devices connected.");
        return;
    }

    String shaderNames = "";
    for (const auto& shader : shaderManager.shaders) {
        shaderNames += shader.first + ";"; // Use semicolon as a delimiter
    }

	Serial.println(shaderNames);

    pNotifyCharacteristic->setValue(shaderNames.c_str());
    pNotifyCharacteristic->notify();
}

class MyCallbacks : public BLECharacteristicCallbacks {

	void onWrite(BLECharacteristic* pCharacteristic) {
		std::string value = pCharacteristic->getValue();

		if (value == "getShaders") {
			sendShaderList();  // Send the list when commanded
		} 
		// else if (value == "getServoSpeeds") {
        //     std::string speeds = std::to_string(servo_1_speed) + ";" +
        //                          std::to_string(servo_2_speed) + ";" +
        //                          std::to_string(servo_3_speed) + ";" +
        //                          std::to_string(servo_master_speed);
        //     pCharacteristic->setValue(speeds);
        //     pCharacteristic->notify();
		// } 
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
			// else if (cmd == "setServoSpeed") {
			// 	            // Assume the value is formatted like "servo1,90"
			// 	int pos = arg.find(";");
			// 	if (pos != std::string::npos) {
			// 		std::string servo = arg.substr(0, pos);
			// 		float speed = std::stof(arg.substr(pos + 1));
			// 		if (servo == "servo1") servo_1_speed = speed;
			// 		else if (servo == "servo2") servo_2_speed = speed;
			// 		else if (servo == "servo3") servo_3_speed = speed;
			// 		else if (servo == "servoMaster") servo_master_speed = speed;
			// 	}
			// } 
			else {
				Serial.println("Invalid command");
			}
		}
	}

};

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* server) {
        Serial.println("Device connected");
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
	TaskHandle_t bluetoothTaskHandle;
	xTaskCreatePinnedToCore(
		bluetoothTask,          // Task function
		"BluetoothTask",        // Name of the task
		10000,                  // Stack size in words
		NULL,                   // Task input parameter
		1,                      // Priority of the task
		&bluetoothTaskHandle,   // Task handle
		1                       // Core number
	);
}

