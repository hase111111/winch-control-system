"""
ロードセルアンプ DSCB シリーズからシリアル通信で連続出力を取得し，
matplotlib で表示するサンプルコード．
Windows，Raspberry Pi OS 環境で動作確認済み．
"""

import serial
import time
import matplotlib.pyplot as plt

# シリアルポートの名前を入力させる．
port_name = input("Enter the serial port name (e.g., COM6 or /dev/ttyUSB0): ")

# ==========================
# シリアル設定
# ==========================
ser = serial.Serial(
    port=port_name,                # ← 環境に合わせて
    baudrate=115200,
    bytesize=serial.EIGHTBITS,
    parity=serial.PARITY_EVEN,
    stopbits=serial.STOPBITS_ONE,
    timeout=1.0
)

# ==========================
# 連続送信開始
# ==========================
ser.write(b'NO99CX\r')
time.sleep(0.1)

t_list = []
load_list = []

t_start = time.time()
print("Start continuous read (100 Hz)")

# ==========================
# 10秒間 取得
# ==========================
while True:
    now = time.time()
    if now - t_start > 10.0:
        break

    line = ser.readline()
    if not line:
        continue

    try:
        text = line.decode('ascii').strip()
        # 例: DSCB01,+12.345
        value = float(text.split(',')[1])
    except Exception:
        continue

    t = now - t_start
    t_list.append(t)
    load_list.append(value)

    print(f"{t:6.3f} s : {value:8.4f} kN")

# ==========================
# 連続送信停止
# ==========================
ser.write(b'NOCY\r')
ser.close()

print("Finished acquisition")

# ==========================
# matplotlib 表示
# ==========================
plt.figure(figsize=(8, 4))
plt.plot(t_list, load_list)
plt.xlabel("Time [s]")
plt.ylabel("Load [kN]")
plt.title("DSCB Continuous Output (100 Hz)")
plt.grid(True)
plt.tight_layout()
plt.show()
