//
//  ContentView.swift
//  gyroled
//
//  Created by Ben Bartlett on 5/10/24.
//

import SwiftUI

//struct ContentView: View {
//    @ObservedObject var bluetoothManager = BluetoothManager()
//
//    var body: some View {
//        VStack(spacing: 20) {
//            Text("gyroled totem controller")
//                .font(.title2)
//                .fontWeight(.bold)
//
//            Button("Value 1") {
//                bluetoothManager.send(value: "Value 1")
//            }
//            Button("Value 2") {
//                bluetoothManager.send(value: "Value 2")
//            }
//            Button("Value 3") {
//                bluetoothManager.send(value: "Value 3")
//            }
//            Button("Value 4") {
//                bluetoothManager.send(value: "Value 4")
//            }
//        }
//        .padding()
//    }
//}

//class ShaderViewModel: ObservableObject {
//    @Published var shaders: [String] = []
//
//    init() {
//        NotificationCenter.default.addObserver(forName: NSNotification.Name("UpdateShaderList"), object: nil, queue: .main) { [weak self] notification in
//            if let shaders = notification.userInfo?["shaders"] as? [String] {
//                self?.shaders = shaders
//            }
//        }
//    }
//}

class TotemViewModel: ObservableObject {
    @Published var shaderNames: [String] = []
    @Published var servo1Speed: Double = 0
    @Published var servo2Speed: Double = 0
    @Published var servo3Speed: Double = 0
    @Published var servoMasterSpeed: Double = 0
}


struct ContentView: View {
    @ObservedObject var viewModel = TotemViewModel()
    @ObservedObject var bluetoothManager: BluetoothManager

    var body: some View {
        ScrollView {
            VStack {
                Text("gyroled totem controller")
                    .font(.title2)
                    .fontWeight(.bold)
                
                ForEach(viewModel.shaderNames, id: \.self) { name in
                    Button(name) {
                        bluetoothManager.sendCommand(cmd: "setActiveShader:" + name)
                    }
                    .padding()
                    .background(Color.blue)
                    .foregroundColor(.white)
                    .cornerRadius(5)
                }
                Slider(value: $viewModel.servo1Speed, in: -0.0...1.0)
                    .onChange(of: viewModel.servo1Speed) {
                        bluetoothManager.sendServoSpeed(servo: 1, speed: viewModel.servo1Speed)
                    }
                Slider(value: $viewModel.servo2Speed, in: -0.0...1.0)
                    .onChange(of: viewModel.servo2Speed) {
                        bluetoothManager.sendServoSpeed(servo: 2, speed: viewModel.servo2Speed)
                    }
                Slider(value: $viewModel.servo3Speed, in: -0.0...1.0)
                    .onChange(of: viewModel.servo3Speed) {
                        bluetoothManager.sendServoSpeed(servo: 3, speed: viewModel.servo3Speed)
                    }
                Slider(value: $viewModel.servoMasterSpeed, in: -0.0...1.0)
                    .onChange(of: viewModel.servoMasterSpeed) {
                        bluetoothManager.sendServoSpeed(servo: 0, speed: viewModel.servoMasterSpeed)
                    }
            }
        }

    }
}





//struct ContentView: View {
//    @ObservedObject var bluetoothManager = BluetoothManager()
//    @ObservedObject var viewModel = ShaderViewModel()
//
//    var body: some View {
//        VStack(spacing: 20) {
//            Text("gyroled totem controller")
//                .font(.title2)
//                .fontWeight(.bold)
//            
//            ForEach(viewModel.shaders, id: \.self) { shader in
//                Button(shader) {
//                    // Implement action to set shader
//                }
//            }
//        }
//    }
//}



//struct ContentView_Previews: PreviewProvider {
//    static var previews: some View {
//        ContentView(BluetoothManager())
//    }
//}
//
//#Preview {
//    ContentView(BluetoothManager())
//}
