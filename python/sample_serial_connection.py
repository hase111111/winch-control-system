"""
USBシリアル通信を行うサンプルコード．
Windows環境で動作確認済み．
"""

import serial

PORT = "COM7"      # ← 自分の環境に合わせる
BAUDRATE = 115200  # Pico側はUSBなので形式的

ser = serial.Serial(PORT, BAUDRATE, timeout=1)

print("Serial opened")

while True:
    line = ser.readline().decode('utf-8').strip()
    if line:
        print("RX:", line)
