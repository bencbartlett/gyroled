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

class ShaderViewModel: ObservableObject {
    @Published var shaderNames: [String] = []
}


struct ContentView: View {
    @ObservedObject var viewModel = ShaderViewModel()
    var bluetoothManager: BluetoothManager

    var body: some View {
        ScrollView {
            VStack {
                Text("gyroled totem controller")
                    .font(.title2)
                    .fontWeight(.bold)
                
                ForEach(viewModel.shaderNames, id: \.self) { name in
                    Button(name) {
                        bluetoothManager.send(value: "setActiveShader:" + name)
                    }
                    .padding()
                    .background(Color.blue)
                    .foregroundColor(.white)
                    .cornerRadius(5)
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
