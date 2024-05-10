//
//  ContentView.swift
//  gyroled
//
//  Created by Ben Bartlett on 5/10/24.
//

import SwiftUI

//struct ContentView: View {
//    var body: some View {
//        VStack {
//            Image(systemName: "globe")
//                .imageScale(.large)
//                .foregroundStyle(.tint)
//            Text("Hello, world!")
//        }
//        .padding()
//    }
//}

struct ContentView: View {
    var body: some View {
        VStack(spacing: 20) {
            Text("gyroled totem controller")
                .font(.title2)
                .fontWeight(.bold)
            
            Button("Value 1") {
                // Action for Value 1
                sendBluetoothValue(value: "Value 1")
            }
            Button("Value 2") {
                // Action for Value 2
                sendBluetoothValue(value: "Value 2")
            }
            Button("Value 3") {
                // Action for Value 3
                sendBluetoothValue(value: "Value 3")
            }
            Button("Value 4") {
                // Action for Value 4
                sendBluetoothValue(value: "Value 4")
            }
        }
        .padding()
    }
    
    func sendBluetoothValue(value: String) {
        print("Send \(value) to ESP32")
        // Here you will integrate Bluetooth communication code
        // This part needs to be filled in with the actual Bluetooth communication logic
    }
}

struct ContentView_Previews: PreviewProvider {
    static var previews: some View {
        ContentView()
    }
}

#Preview {
    ContentView()
}
