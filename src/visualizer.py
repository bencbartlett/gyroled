import sys
import serial
import time
from PyQt5.QtWidgets import QApplication, QMainWindow
from PyQt5.QtCore import QTimer
from PyQt5.QtGui import QBrush

import pyqtgraph as pg

# Adjust the following line to your serial port
serialPort = '/dev/cu.usbserial-0001'
baudRate = 115200

class MainWindow(QMainWindow):
    def __init__(self, parent=None):
        super(MainWindow, self).__init__(parent)

        self.serial = serial.Serial(serialPort, baudRate, timeout=1)
        self.setWindowTitle('EQ Visualization')

        self.graphWidget = pg.PlotWidget()
        self.setCentralWidget(self.graphWidget)

        self.bars = 7
        self.bgColor = 'w'
        # self.barColors = [QBrush(pg.mkColor(color)) for color in [(255, 0, 0), (0, 255, 0), (0, 0, 255), (255, 255, 0), (255, 0, 255), (0, 255, 255), (128, 128, 128)]]
        self.x = list(range(self.bars))
        self.y = [0] * self.bars

        # Set a consistent y-axis range
        self.graphWidget.setYRange(min=0, max=20)  # Adjust min and max values as needed


        self.graphWidget.setBackground(self.bgColor)
        # self.barGraphItem = pg.BarGraphItem(x=self.x, height=self.y, width=0.6, brush=self.barColors)
        self.barGraphItem = pg.BarGraphItem(x=self.x, height=self.y, width=0.6, brush='r')

        self.graphWidget.addItem(self.barGraphItem)

        self.timer = QTimer()
        self.timer.setInterval(10)  # in milliseconds
        self.timer.timeout.connect(self.update)
        self.timer.start()

        self.lastUpdate = time.time()

    def update(self):
        if self.serial.inWaiting() > 0:
            line = self.serial.readline().decode('utf-8').strip()
            parts = line.split(',')
            if len(parts) == self.bars:
                self.y = [int(val) for val in parts]
                self.barGraphItem.setOpts(height=self.y)
            print(f"Last update: {time.time() - self.lastUpdate:.2f} s")
            print(f"Frames per second: {1 / (time.time() - self.lastUpdate):.2f}")
            self.lastUpdate = time.time()

if __name__ == '__main__':
    app = QApplication(sys.argv)
    window = MainWindow()
    window.show()
    sys.exit(app.exec_())
