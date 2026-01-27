from machine import ADC, Pin
import network
import socket
import time

# =========================
# ADC 設定
# =========================
adc0 = ADC(26)  # GPIO26
adc1 = ADC(27)  # GPIO27（使わないなら消してOK）
adc2 = ADC(28)  # GPIO28

VREF = 3.3
CONV = VREF / 65535

# =========================
# Wi-Fi（STA）設定
# =========================
SSID = "aterm-5d78db-g"
PASSWORD = "5be63f163242f"
PC_IP = "192.168.0.230"
PORT = 5005

wlan = network.WLAN(network.STA_IF)
wlan.active(True)

if not wlan.isconnected():
    print("Connecting to Wi-Fi...")
    wlan.connect(SSID, PASSWORD)
    while not wlan.isconnected():
        time.sleep(0.5)

print("Wi-Fi connected")
print("IP config:", wlan.ifconfig())

# =========================
# UDP 設定
# =========================
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
DEST = (PC_IP, PORT)

# =========================
# メインループ
# =========================
while True:
    # ADC 読み取り
    v0 = adc0.read_u16() * CONV
    v2 = adc2.read_u16() * CONV

    # 送信メッセージ（CSV形式）
    msg = f"{v0:.3f},{v2:.3f}\n"

    # UDP送信
    sock.sendto(msg.encode("utf-8"), DEST)

    # デバッグ表示
    print("Sent:", msg.strip())

    time.sleep(0.01)  # 100 Hz
