//
//  gyroledApp.swift
//  gyroled
//
//  Created by Ben Bartlett on 5/10/24.
//

import SwiftUI
import CoreBluetooth

@main
struct gyroledApp: App {
    var body: some Scene {
        WindowGroup {
            ContentView()
        }
    }
}

class BluetoothManager: NSObject, ObservableObject, CBCentralManagerDelegate, CBPeripheralDelegate {
    var centralManager: CBCentralManager!
    var espPeripheral: CBPeripheral?
    var ledCharacteristic: CBCharacteristic?

    let serviceUUID = CBUUID(string: "4fafc201-1fb5-459e-8fcc-c5c9c331914b")
    let characteristicUUID = CBUUID(string: "beb5483e-36e1-4688-b7f5-ea07361b26a8")

    override init() {
        super.init()
        centralManager = CBCentralManager(delegate: self, queue: nil)
    }

    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        if central.state == .poweredOn {
            centralManager.scanForPeripherals(withServices: [serviceUUID], options: nil)
        } else {
            print("Bluetooth is not available.")
        }
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
        if let services = peripheral.services {
            for service in services where service.uuid == serviceUUID {
                peripheral.discoverCharacteristics([characteristicUUID], for: service)
            }
        }
    }

    func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
        if let characteristics = service.characteristics {
            for characteristic in characteristics where characteristic.uuid == characteristicUUID {
                ledCharacteristic = characteristic
            }
        }
    }

    func send(value: String) {
        guard let characteristic = ledCharacteristic else {
            print("Characteristic not found.")
            return
        }
        if let data = value.data(using: .utf8) {
            espPeripheral?.writeValue(data, for: characteristic, type: .withResponse)
        }
    }
}


