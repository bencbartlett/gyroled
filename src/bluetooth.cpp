#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include "shaders.hpp"

// UUIDs for BLE service and characteristic
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

extern ShaderManager shaderManager;
String receivedValue = "None";

class MyCallbacks : public BLECharacteristicCallbacks {
	void onWrite(BLECharacteristic* pCharacteristic) {
		std::string value = pCharacteristic->getValue();

		if (value.length() > 0) {
			Serial.println("Received Value: ");
			for (int i = 0; i < value.length(); i++)
				Serial.print(value[i]);
			Serial.println();

			// Update the receivedValue with the new value
			receivedValue = value.c_str();

			// Set the active shader based on received value
			shaderManager.setActiveShader(receivedValue);
		}
	}
};

void bluetoothTask(void* args) {
	// Initialize BLE, called by setup() in main.cpp
	BLEDevice::init("Gyroled Totem");

	BLEServer* pServer = BLEDevice::createServer();
	BLEService* pService = pServer->createService(SERVICE_UUID);
	BLECharacteristic* pCharacteristic = pService->createCharacteristic(
		CHARACTERISTIC_UUID,
		BLECharacteristic::PROPERTY_READ |
		BLECharacteristic::PROPERTY_WRITE
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

