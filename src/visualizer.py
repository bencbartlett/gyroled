import sys
import serial
import time
from PyQt5.QtWidgets import QApplication, QMainWindow
from PyQt5.QtCore import QTimer
from PyQt5.QtGui import QBrush

import pyqtgraph as pg

import numpy as np

# Adjust the following line to your serial port
# serialPort = '/dev/cu.usbserial-0001'
serialPort = '/dev/cu.usbmodem2101' # right usb-c port on my MBP
baudRate = 115200

class MainWindow(QMainWindow):
    def __init__(self, parent=None):
        super(MainWindow, self).__init__(parent)

        self.serial = serial.Serial(serialPort, baudRate, timeout=1)
        self.setWindowTitle('EQ Visualization')

        self.graphWidget = pg.PlotWidget()
        self.setCentralWidget(self.graphWidget)

        self.bars = 9
        self.bgColor = 'w'
        # self.barColors = [QBrush(pg.mkColor(color)) for color in [(255, 0, 0), (0, 255, 0), (0, 0, 255), (255, 255, 0), (255, 0, 255), (0, 255, 255), (128, 128, 128)]]
        self.x = list(range(self.bars))
        self.y = [0] * self.bars

        self.max_y = 1.0
        self.max_heuristic = 1.0
        
        self.heuristics = [0.01 for _ in range(40 * 3)]  # 3 seconds of data

        # Set a consistent y-axis range
        self.graphWidget.setYRange(min=0, max=3000)  # Adjust min and max values as needed


        self.graphWidget.setBackground(self.bgColor)
        # self.barGraphItem = pg.BarGraphItem(x=self.x, height=self.y, width=0.6, brush=self.barColors)
        self.barGraphItem = pg.BarGraphItem(x=self.x, height=self.y, width=0.6, brush='r')

        self.graphWidget.addItem(self.barGraphItem)

        self.timer = QTimer()
        self.timer.setInterval(10)  # in milliseconds
        self.timer.timeout.connect(self.update)
        self.timer.start()

        self.avg_energy = 10.0
        self.prev_energy = 0.0
        self.light_value = 0.0

        self.lastUpdate = time.time()

    def update(self):
        updated = False
        if self.serial.inWaiting() > 0:
            line = self.serial.readline().decode('utf-8').strip()
            try:
                if "[SPECTROGRAM]:" in line:
                    line = line.replace("[SPECTROGRAM]:", "")
                    try:
                        bands, predicate = line.split(';')
                        bands = bands.split(',')
                    except:
                        print(line)
                        raise ValueError
                    # if len(parts) == self.bars:
                    new_y = [float(band) for band in bands]
                    # new_y[0] = 0.0
                    # print(len(new_y))

                    energy = sum([new_y[i]**2 / (i+1) for i, _ in enumerate(new_y)])
                    energy = new_y[0]


                    alpha = 0.1
                    self.avg_energy = alpha * energy + (1 - alpha) * self.avg_energy
                    intensity = ((energy - self.avg_energy) / (self.avg_energy + 0.001)) ** 2

                    heuristic = float(predicate)
                    # self.avg_heuristic = alpha * heuristic + (1 - alpha) * self.avg_heuristic

                    if heuristic > self.max_heuristic:
                        self.max_heuristic = heuristic

                    self.heuristics = self.heuristics[1:] + [heuristic]

                    # self.light_value -= 0.5
                    # if intensity > self.light_value:
                    #     self.light_value = intensity
                    # if self.light_value < 0:
                    #     self.light_value = 0

                    # self.light_value -= 0.5
                    # if energy > self.light_value:
                    #     self.light_value = intensity
                    # if self.light_value < 0:
                    #     self.light_value = 0

                    mean_heuristic = sum(self.heuristics) / len(self.heuristics)

                    if heuristic / mean_heuristic > self.light_value and heuristic > np.percentile(self.heuristics, 90):
                        self.light_value = min(heuristic / mean_heuristic, 5)
                    else:
                        self.light_value -= (5/0.25) / (40)
                    self.light_value = max(0, self.light_value)

                    # print(self.light_value)
                        

                    # if intensity < 0.1:
                    #     intensity = 0
                    # else:
                    #     intensity -= 0.01
                    #     intensity = (1 + intensity) ** 3 - 1
                    #     intensity = max(0, min(1, intensity))
                    new_y += [self.light_value * 500]
                    if new_y != self.y:
                        updated = True
                        self.y = new_y
                        self.prev_energy = energy
                        if max(new_y) > self.max_y:
                            self.max_y = max(new_y)
                            # self.graphWidget.setYRange(min=0, max=self.max_y + 100)
                    self.barGraphItem.setOpts(height=self.y)
                    # print(f"Last update: {time.time() - self.lastUpdate:.2f} s")
                    print(f"Frames per second: {1 / (time.time() - self.lastUpdate):.2f}")
                    if updated:
                        self.lastUpdate = time.time()
            except:
                pass

if __name__ == '__main__':
    app = QApplication(sys.argv)
    window = MainWindow()
    window.show()
    sys.exit(app.exec_())
