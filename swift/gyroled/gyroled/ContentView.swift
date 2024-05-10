//
//  ContentView.swift
//  gyroled
//
//  Created by Ben Bartlett on 5/10/24.
//

import SwiftUI

struct ContentView: View {
    @ObservedObject var bluetoothManager = BluetoothManager()

    var body: some View {
        VStack(spacing: 20) {
            Text("gyroled totem controller")
                .font(.title2)
                .fontWeight(.bold)

            Button("Value 1") {
                bluetoothManager.send(value: "Value 1")
            }
            Button("Value 2") {
                bluetoothManager.send(value: "Value 2")
            }
            Button("Value 3") {
                bluetoothManager.send(value: "Value 3")
            }
            Button("Value 4") {
                bluetoothManager.send(value: "Value 4")
            }
        }
        .padding()
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
