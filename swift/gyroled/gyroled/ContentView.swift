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
    @Published var activeShader: String = "Loopy Rainbow"
    @Published var activeAccentShader: String = "White Peaks"
    @Published var servo1Speed: Double = 0.7
    @Published var servo2Speed: Double = 0.8
    @Published var servo3Speed: Double = 1.0
    @Published var servoMasterSpeed: Double = 0.3
}


//struct ContentView: View {
//    @ObservedObject var viewModel: TotemViewModel
//    @ObservedObject var bluetoothManager: BluetoothManager
//
//    var body: some View {
//        ScrollView {
//            VStack {
//                Text("GyroLED Totem Controller")
//                    .font(.title2)
//                    .fontWeight(.bold)
//                    .padding(.bottom, 20) // Add some space below the title
//
//                ForEach(viewModel.shaderNames, id: \.self) { name in
//                    Button(name) {
//                        bluetoothManager.sendCommand(cmd: "setActiveShader:" + name)
//                    }
//                    .padding()
//                    .frame(width: 300) // Uniform width for all buttons
//                    .background(Color.blue)
//                    .foregroundColor(.white)
//                    .cornerRadius(5)
//                }
//
//                VStack(alignment: .leading) {
//                    Text("Servo 1 Speed")
//                    Slider(value: $viewModel.servo1Speed, in: -0.0...1.0)
//                        .onChange(of: viewModel.servo1Speed) {
//                            bluetoothManager.sendServoSpeed(servo: 1, speed: viewModel.servo1Speed)
//                        }
//                }
//                .padding(.horizontal, 30)
//
//                VStack(alignment: .leading) {
//                    Text("Servo 2 Speed")
//                    Slider(value: $viewModel.servo2Speed, in: -0.0...1.0)
//                        .onChange(of: viewModel.servo2Speed) {
//                            bluetoothManager.sendServoSpeed(servo: 2, speed: viewModel.servo2Speed)
//                        }
//                }
//                .padding(.horizontal, 30)
//
//                VStack(alignment: .leading) {
//                    Text("Servo 3 Speed")
//                    Slider(value: $viewModel.servo3Speed, in: -0.0...1.0)
//                        .onChange(of: viewModel.servo3Speed) {
//                            bluetoothManager.sendServoSpeed(servo: 3, speed: viewModel.servo3Speed)
//                        }
//                }
//                .padding(.horizontal, 30)
//
//                VStack(alignment: .leading) {
//                    Text("Master Servo Speed")
//                    Slider(value: $viewModel.servoMasterSpeed, in: -0.0...1.0)
//                        .onChange(of: viewModel.servoMasterSpeed) {
//                            bluetoothManager.sendServoSpeed(servo: 0, speed: viewModel.servoMasterSpeed)
//                        }
//                }
//                .padding(.horizontal, 30)
//
//            }
//            .padding(.top, 20) // Add padding at the top of the VStack within the ScrollView
//        }
//    }
//}
//
//import SwiftUI

struct ContentView: View {
    @ObservedObject var viewModel: TotemViewModel
    @ObservedObject var bluetoothManager: BluetoothManager

    var body: some View {
        
        NavigationView {
            Form {
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

                // Servo control section
                Section(header: Text("Servo Controls")) {
                    sliderView(title: "Servo1 Speed", servoIndex: 1, value: $viewModel.servo1Speed)
                    sliderView(title: "Servo2 Speed", servoIndex: 2, value: $viewModel.servo2Speed)
                    sliderView(title: "Servo3 Speed", servoIndex: 3, value: $viewModel.servo3Speed)
                    sliderView(title: "Master Speed", servoIndex: 0, value: $viewModel.servoMasterSpeed)
                }

                // Beat drop section
                Section {
                    Button(action: {
                        bluetoothManager.sendCommand(cmd: "beatDrop")
                    }) {
                        Text("🚨BEAT DROP🚨")
                            .fontWeight(.bold)
                            .foregroundColor(.white)
                            .padding()
                            .frame(maxWidth: .infinity)
                            .background(Color.red)
                            .cornerRadius(8)
                    }
                }
            }
            .navigationBarTitle("GyroLED Totem Controller", displayMode: .inline)
        }
        
        
        
//        ScrollView {
//            VStack {
//                headerText
//                Text("Shaders")
//                    .multilineTextAlignment(/*@START_MENU_TOKEN@*/.leading/*@END_MENU_TOKEN@*/)
//                    .font(.title3)
//                    .fontWeight(.bold)
//                
//                // Shader selection section
//                Section(header: Text("Shader Selection")) {
//                    // Picker for shader selection
//                    Picker("Shader", selection: $viewModel.activeShader) {
//                        ForEach(viewModel.shaderNames, id: \.self) { name in
//                            Text(name).tag(name)
//                        }
//                    }
//                    .pickerStyle(InlinePickerStyle())
//                    .onChange(of: viewModel.activeShader) {
//                        bluetoothManager.setActiveShader(shader: viewModel.activeShader)
//                    }
//                }
//                
//                Section(header: Text("Accent Selection")) {
//                    // Picker for shader selection
//                    Picker("AccentShader", selection: $viewModel.activeAccentShader) {
//                        ForEach(viewModel.accentShaderNames, id: \.self) { name in
//                            Text(name).tag(name)
//                        }
//                    }
//                    .pickerStyle(InlinePickerStyle())
//                    .onChange(of: viewModel.activeAccentShader) {
//                        bluetoothManager.setActiveAccentShader(shader: viewModel.activeAccentShader)
//                    }
//                }
//                
//                
//                
////                
////                Text("Accents")
////                    .multilineTextAlignment(/*@START_MENU_TOKEN@*/.leading/*@END_MENU_TOKEN@*/)
////                    .font(.title3)
////                    .fontWeight(.bold)
//                
//                // Picker for shader selection
//                Picker("Shader", selection: $viewModel.activeAccentShader) {
//                    ForEach(viewModel.accentShaderNames, id: \.self) { name in
//                        Text(name).tag(name)
//                    }
//                }
//                .pickerStyle(.inline) // You can choose .inline, .wheel, .segmented based on your UI needs
//                .frame(height: 150)  // Adjust height as needed
//                .clipped()
//
//                // Trigger update when selection changes
//                .onChange(of: viewModel.activeAccentShader) {
//                    bluetoothManager.sendCommand(cmd: "setActiveAccentShader:" + viewModel.activeAccentShader)
//                }
//                
//                
////                accentShaderList
//                slidersGroup
//                // Beat Drop Button
//                Button(action: {
//                    bluetoothManager.sendCommand(cmd: "beatDrop")
//                }) {
//                    Text("BEAT DROP")
//                        .fontWeight(.bold)
//                        .foregroundColor(.white)
//                        .padding()
//                        .frame(maxWidth: 300)  // Makes the button stretch to the full width
//                        .background(Color.red)
//                        .cornerRadius(8)
//                }
//                .padding()  // Adds padding around the button for better spacing
//            }
//            .padding(.top, 20)  // Padding at the top of the VStack
//        }
    }

    private var headerText: some View {
        Text("GyroLED Totem Controller")
            .font(.title2)
            .fontWeight(.bold)
            .padding(.bottom, 20)  // Space below the title
    }
    
    @State var activeShader: String = "Loopy Rainbow"

//    private var shaderList: some View {
//        List {
//            ForEach(viewModel.shaderNames, id: \.self) { name in
//                SelectionCell(shader: name, activeShader: self.$activeShader)
//                    .onTapGesture(perform: {
//                        bluetoothManager.setActiveShader(shader: name)
//                    })
//            }
//        }
//        .navigationTitle("Shaders")
//        .frame(height: 250)  // Set a fixed height for the list
//    }
//    
//    private var accentShaderList: some View {
//        List {
//            ForEach(viewModel.accentShaderNames, id: \.self) { name in
//                SelectionCell(shader: name, activeShader: self.$activeShader)
//                    .onTapGesture(perform: {
//                        bluetoothManager.setActiveAccentShader(shader: name)
//                    })
//            }
//        }
//        .navigationTitle("Accent Shaders")
//        .frame(height: 150)  // Set a fixed height for the list
//    }

    @ViewBuilder
    private func sliderView(title: String, servoIndex: Int, value: Binding<Double>) -> some View {
//        VStack(alignment: .leading) {
            HStack {
                Text(title)
//                Spacer()
                Slider(value: value, in: -0.0...1.0)
                    .onChange(of: value.wrappedValue) {
                        bluetoothManager.sendServoSpeed(servo: servoIndex, speed: value.wrappedValue)
                    }
                Text(String(format: "%.2f", value.wrappedValue))
            }
//            Slider(value: value, in: -0.0...1.0)
//                .onChange(of: value.wrappedValue) {
//                    bluetoothManager.sendServoSpeed(servo: servoIndex, speed: value.wrappedValue)
//                }
//        }
//        .padding(.vertical, 5)
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

//struct ShaderRow: View {
//    let name: String
//    let isActive: Bool
//    let onSelect: () -> Void
//
//    var body: some View {
//        Text(name)
//            .padding()
//            .background(isActive ? Color.blue : Color.clear)
//            .foregroundColor(isActive ? .white : .black)
//            .onTapGesture(perform: onSelect)
//    }
//}


//struct ContentView_Previews: PreviewProvider {
//    static var previews: some View {
//        ContentView(viewModel: TotemViewModel(), bluetoothManager: BluetoothManager())
//    }
//}
