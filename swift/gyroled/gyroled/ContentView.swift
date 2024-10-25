//
//  ContentView.swift
//  gyroled
//
//  Created by Ben Bartlett on 5/10/24.
//

import SwiftUI

class TotemViewModel: ObservableObject {
    @Published var shaderNames: [String] = []
    @Published var accentShaderNames: [String] = []
    @Published var isAnimationActive: Bool = false
    @Published var activeShader: String = "Loopy Rainbow"
    @Published var activeAccentShader: String = "(No Accent)"
    @Published var brightness: Int = 255
    @Published var servo1Speed: Double = 0.7
    @Published var servo2Speed: Double = 0.8
    @Published var servo3Speed: Double = 1.0
    @Published var servoMasterSpeed: Double = 0.3
}

struct ContentView: View {
    @ObservedObject var viewModel: TotemViewModel
    @ObservedObject var bluetoothManager: BluetoothManager

    var body: some View {
        
        NavigationView {
            Form {
                
                // Servo control section
                Section(header: Text("Hardware Controls")) {
                    Toggle("Enable Animation", isOn: $viewModel.isAnimationActive)
                        .onChange(of: viewModel.isAnimationActive) {
                            if viewModel.isAnimationActive {
                                bluetoothManager.activateAnimation()
                            } else {
                                bluetoothManager.deactivateAnimation()
                            }
                        }
                    HStack {
                        Text("Brightness    ")
                        Slider(value: Binding(
                            get: { Double(viewModel.brightness) },
                            set: { newValue in viewModel.brightness = Int(newValue) }
                        ), in: 0...255, step: 1) {
                            Text("Brightness    ")
                        } onEditingChanged: { _ in
                            bluetoothManager.setBrightness(brightness: viewModel.brightness)
                        }
                        Text(String(Int(viewModel.brightness)).padding(toLength: 4, withPad: " ", startingAt: 0))
                    }
                    sliderView(title: "Servo1 Speed", servoIndex: 1, value: $viewModel.servo1Speed)
                    sliderView(title: "Servo2 Speed", servoIndex: 2, value: $viewModel.servo2Speed)
                    sliderView(title: "Servo3 Speed", servoIndex: 3, value: $viewModel.servo3Speed)
                    sliderView(title: "Master Speed", servoIndex: 0, value: $viewModel.servoMasterSpeed)
                }
                
                // Shader selection section
                Section(header: Text("Shader")) {
                    // Picker for shader selection
                    Picker(selection: $viewModel.activeShader) {
                        ForEach(viewModel.shaderNames, id: \.self) { name in
                            Text(name).tag(name)
                        }
                    } label: {}
                    .pickerStyle(InlinePickerStyle())
                    .onChange(of: viewModel.activeShader) {
                        bluetoothManager.setActiveShader(shader: viewModel.activeShader)
                    }
                }
                
                Section(header: Text("Accent Shader")) {
                    // Picker for shader selection
                    Picker(selection: $viewModel.activeAccentShader) {
                        ForEach(viewModel.accentShaderNames, id: \.self) { name in
                            Text(name).tag(name)
                        }
                    } label: {}
                    .pickerStyle(InlinePickerStyle())
                    .onChange(of: viewModel.activeAccentShader) {
                        bluetoothManager.setActiveAccentShader(shader: viewModel.activeAccentShader)
                    }
                }

                // Beat drop section
//                Section(header: Text("Daaaanger zone!")) {
//                    Button(action: {
//                        bluetoothManager.sendCommand(cmd: "beatDrop")
//                    }) {
//                        Text("🚨BEAT DROP🚨")
//                            .fontWeight(.bold)
//                            .foregroundColor(.white)
//                            .padding()
//                            .frame(maxWidth: .infinity)
//                            .background(Color.red)
//                            .cornerRadius(8)
//                    }
//                }
                
                Button(action: {
                    bluetoothManager.reconnect()
                }) {
                    Text("Reconnect")
//                        .padding()
//                        .frame(maxWidth: .infinity)
//                        .cornerRadius(8)
                }
            }
            .navigationBarTitle("GyroLED Totem Controller", displayMode: .inline)
        }
        
    }

    private var headerText: some View {
        Text("GyroLED Totem Controller")
            .font(.title2)
            .fontWeight(.bold)
            .padding(.bottom, 20)  // Space below the title
    }
    
    @State var activeShader: String = "Loopy Rainbow"

    @ViewBuilder
    private func sliderView(title: String, servoIndex: Int, value: Binding<Double>) -> some View {
            HStack {
                Text(title)
                Slider(value: value, in: -0.0...1.0)
                    .onChange(of: value.wrappedValue) {
                        bluetoothManager.sendServoSpeed(servo: servoIndex, speed: value.wrappedValue)
                    }
                Text(String(format: "%.2f", value.wrappedValue))
            }
    }
}

struct SelectionCell: View {

    let shader: String
    @Binding var activeShader: String

    var body: some View {
        HStack {
            Text(shader)
            Spacer()
            if shader == activeShader {
                Image(systemName: "checkmark")
                    .foregroundColor(.accentColor)
            }
        }   .onTapGesture {
                self.activeShader = self.shader
            }
    }
}

//struct ContentView_Previews: PreviewProvider {
//    static var previews: some View {
//        ContentView(viewModel: TotemViewModel(), bluetoothManager: BluetoothManager())
//    }
//}
