//
//  gyroledApp.swift
//  gyroled
//
//  Created by Ben Bartlett on 5/10/24.
//

import SwiftUI
import CoreBluetooth

//// ViewModel to hold shader names
//class ShaderViewModel: ObservableObject {
//    @Published var shaderNames: [String] = []
//}

// Bluetooth manager for handling BLE operations
class BluetoothManager: NSObject, ObservableObject, CBCentralManagerDelegate, CBPeripheralDelegate {
    var centralManager: CBCentralManager!
    var espPeripheral: CBPeripheral?
    var ledCharacteristic: CBCharacteristic?

    // Define the UUIDs for services and characteristics
    let serviceUUID = CBUUID(string: "4fafc201-1fb5-459e-8fcc-c5c9c331914b")
    let characteristicUUID = CBUUID(string: "beb5483e-36e1-4688-b7f5-ea07361b26a8")
    let notifyCharacteristicUUID = CBUUID(string: "39f10f90-c4ee-d0fc-6dec-cbc5cfff5a9b")

    @Published var viewModel = ShaderViewModel()

    override init() {
        super.init()
        centralManager = CBCentralManager(delegate: self, queue: nil)
        // Add observers in init or appropriate lifecycle method:
//        NotificationCenter.default.addObserver(forName: UIApplication.willEnterForegroundNotification, object: nil, queue: nil) { [weak self] _ in
//            self?.reconnect()
//        }
    }

//    func centralManagerDidUpdateState(_ central: CBCentralManager) {
//        if central.state == .poweredOn {
//            centralManager.scanForPeripherals(withServices: [serviceUUID], options: nil)
//        } else {
//            print("Bluetooth is not available.")
//        }
//    }
    
//    func centralManagerDidUpdateState(_ central: CBCentralManager) {
//        if central.state == .poweredOn {
//            reconnect()
//        }
//    }
    
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
                requestShaderList(peripheral: peripheral)  // Request shader list
            }
        }
    }

    func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
        guard error == nil else {
            print("Error receiving notification from characteristic \(characteristic): \(error!.localizedDescription)")
            return
        }

        if let value = characteristic.value, let shaderListString = String(data: value, encoding: .utf8) {
            let shaderNames = shaderListString.split(separator: ";").map(String.init)
            print(shaderNames)
            DispatchQueue.main.async {
                self.viewModel.shaderNames = shaderNames
            }
        }
    }
    
    func requestShaderList(peripheral: CBPeripheral) {
        if let characteristic = ledCharacteristic {
            let commandString = "getShaders"
            let data = Data(commandString.utf8)
            peripheral.writeValue(data, for: characteristic, type: .withResponse)
        }
    }
    
    func send(value: String) {
        guard let peripheral = espPeripheral,
              let characteristic = ledCharacteristic else {
            print("Characteristic not found.")
            return
        }

        if let data = value.data(using: .utf8) {
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

