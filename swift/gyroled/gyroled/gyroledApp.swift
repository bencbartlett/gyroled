//
//  gyroledApp.swift
//  gyroled
//
//  Created by Ben Bartlett on 5/10/24.
//

import SwiftUI
import CoreBluetooth

// Bluetooth manager for handling BLE operations
class BluetoothManager: NSObject, ObservableObject, CBCentralManagerDelegate, CBPeripheralDelegate {
    var centralManager: CBCentralManager!
    var espPeripheral: CBPeripheral?
    var ledCharacteristic: CBCharacteristic?

    // Define the UUIDs for services and characteristics
    let serviceUUID = CBUUID(string: "4fafc204-1fb5-459e-8fcc-c5c9c331914b")
    let characteristicUUID = CBUUID(string: "beb5483f-36e1-4688-b7f5-ea07361b26a8")
    let notifyCharacteristicUUID = CBUUID(string: "39f10f91-c4ee-d0fc-6dec-cbc5cfff5a9b")

    @Published var viewModel = TotemViewModel()

    override init() {
        super.init()
        centralManager = CBCentralManager(delegate: self, queue: nil)
        
        // Setup notification observer for app becoming active
        NotificationCenter.default.addObserver(
            self,
            selector: #selector(appDidBecomeActive),
            name: UIApplication.didBecomeActiveNotification,
            object: nil
        )
    }
    
    deinit {
        NotificationCenter.default.removeObserver(self)
    }

    @objc private func appDidBecomeActive() {
        print("App became active - attempting to reconnect.")
        reconnect()
    }
    
    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        switch central.state {
        case .poweredOn:
            print("Bluetooth is powered on.")
            reconnect() // Now safe to call reconnect
        case .resetting, .unauthorized, .unknown, .unsupported, .poweredOff:
            print("Bluetooth is not available or not authorized.")
        @unknown default:
            print("A new state was added that is not handled")
        }
    }


    func reconnect() {
        guard centralManager.state == .poweredOn else {
            print("Central Manager is not powered on.")
            return
        }

        let knownPeripherals = centralManager.retrieveConnectedPeripherals(withServices: [serviceUUID])
        if let peripheral = knownPeripherals.first {
            connectToPeripheral(peripheral)
        } else {
            centralManager.scanForPeripherals(withServices: [serviceUUID], options: nil)
        }
    }

    func connectToPeripheral(_ peripheral: CBPeripheral) {
        espPeripheral = peripheral
        espPeripheral!.delegate = self
        centralManager.connect(espPeripheral!)
    }

    func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral, advertisementData: [String: Any], rssi RSSI: NSNumber) {
        espPeripheral = peripheral
        espPeripheral!.delegate = self
        centralManager.stopScan()
        centralManager.connect(espPeripheral!)
    }

    func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        peripheral.discoverServices([serviceUUID])
    }

    func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        guard let services = peripheral.services else { return }
        for service in services {
            peripheral.discoverCharacteristics(nil, for: service)
        }
    }

    func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
        guard let characteristics = service.characteristics else { return }
        for characteristic in characteristics {
            if characteristic.uuid == notifyCharacteristicUUID {
                peripheral.setNotifyValue(true, for: characteristic) // Set to receive notifications
            }
            if characteristic.uuid == characteristicUUID {
                ledCharacteristic = characteristic
                requestShaderList(peripheral: peripheral)
                requestAccentShaderList(peripheral: peripheral)
                requestActiveShader(peripheral: peripheral)
                requestServoSpeeds(peripheral: peripheral)
                requestActiveAccentShader(peripheral: peripheral)
                requestBrightness(peripheral: peripheral)
                requestIsAnimationActive(peripheral: peripheral)
            }
        }
    }

    func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
        guard error == nil else {
            print("Error receiving notification from characteristic \(characteristic): \(error!.localizedDescription)")
            return
        }

        if let value = characteristic.value, let receivedString = String(data: value, encoding: .utf8) {
            // Determine the type of command received by checking the prefix
            if receivedString.starts(with: "shaders:") {
                let shaderNames = receivedString
                    .dropFirst("shaders:".count)
                    .split(separator: ";").map(String.init)
                DispatchQueue.main.async {
                    self.viewModel.shaderNames = shaderNames
                }
            } else if receivedString.starts(with: "brightness:") {
                let brightnessString = receivedString.dropFirst("brightness:".count)
                if let brightness = Int(brightnessString) {
                    DispatchQueue.main.async {
                        self.viewModel.brightness = brightness
                    }
                } else {
                    print("Failed to convert brightness to Int")
                    // Handle the error or set a default value
                }
            } else if receivedString.starts(with: "activeShader:") {
                let activeShaderName = String(receivedString.dropFirst("activeShader:".count))
                DispatchQueue.main.async {
                    self.viewModel.activeShader = activeShaderName
                }
            }
            else if receivedString.starts(with: "accentShaders:") {
                let accentShaderNames = receivedString
                    .dropFirst("accentShaders:".count)
                    .split(separator: ";").map(String.init)
                DispatchQueue.main.async {
                    self.viewModel.accentShaderNames = accentShaderNames
                }
            } else if receivedString.starts(with: "activeAccentShader:") {
                let activeAccentShaderName = String(receivedString.dropFirst("activeAccentShader:".count))
                DispatchQueue.main.async {
                    self.viewModel.activeAccentShader = activeAccentShaderName
                }
            } else if receivedString.starts(with: "animationState:") {
                let stateString = receivedString.dropFirst("animationState:".count)
                if let state = Bool(String(stateString)) {
                    DispatchQueue.main.async {
                        self.viewModel.isAnimationActive = state
                    }
                } else {
                    print("Failed to parse animation state")
                }
            } else if receivedString.starts(with: "servoSpeeds:") {
                let speeds = receivedString
                    .dropFirst("servoSpeeds:".count)
                    .split(separator: ";")
                    .compactMap { Double($0) }
                
                if speeds.count == 4 {
                    DispatchQueue.main.async {
                        self.viewModel.servoMasterSpeed = speeds[0]
                        self.viewModel.servo1Speed = speeds[1]
                        self.viewModel.servo2Speed = speeds[2]
                        self.viewModel.servo3Speed = speeds[3]
                    }
                } else {
                    print("Received incorrect number of servo speeds")
                }
            }
        }
    }
    
    func requestShaderList(peripheral: CBPeripheral) {
        sendCommand(cmd: "getShaders")
    }
    
    func requestActiveShader(peripheral: CBPeripheral) {
        sendCommand(cmd: "getActiveShader")
    }
    
    func requestAccentShaderList(peripheral: CBPeripheral) {
        sendCommand(cmd: "getAccentShaders")
    }
    
    func requestActiveAccentShader(peripheral: CBPeripheral) {
        sendCommand(cmd: "getActiveAccentShader")
    }
    
    func requestServoSpeeds(peripheral: CBPeripheral) {
        sendCommand(cmd: "getServoSpeeds")
    }

    func sendServoSpeed(servo: Int, speed: Double) {
        sendCommand(cmd: "setServoSpeed:\(servo);\(speed)")
    }
    
    func requestIsAnimationActive(peripheral: CBPeripheral) {
        sendCommand(cmd: "getIsAnimationActive")
    }
    
    func activateAnimation() {
        sendCommand(cmd: "activateAnimation")
    }

    func deactivateAnimation() {
        sendCommand(cmd: "deactivateAnimation")
    }
    
    func setActiveShader(shader: String) {
        sendCommand(cmd: "setActiveShader:\(shader)")
    }
    
    func setActiveAccentShader(shader: String) {
        sendCommand(cmd: "setActiveAccentShader:\(shader)")
    }
    
    func requestBrightness(peripheral: CBPeripheral) {
        sendCommand(cmd: "getBrightness")
    }
    
    func setBrightness(brightness: Int) {
        sendCommand(cmd: "setBrightness:\(brightness)")
    }
    
    func sendCommand(cmd: String) {
        guard let peripheral = espPeripheral,
              let characteristic = ledCharacteristic else {
            print("Characteristic not found.")
            return
        }

        if let data = cmd.data(using: .utf8) {
            peripheral.writeValue(data, for: characteristic, type: .withResponse)
        }
    }

}



// Main App entry point
@main
struct gyroledApp: App {
    var bluetoothManager = BluetoothManager()

    var body: some Scene {
        WindowGroup {
            ContentView(viewModel: bluetoothManager.viewModel, bluetoothManager: bluetoothManager)
        }
    }
}

