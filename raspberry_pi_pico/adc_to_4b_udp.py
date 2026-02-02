from machine import ADC, Pin
import network
import socket
import time

# =========================
# ADC 設定
# =========================
adc0 = ADC(26)
adc2 = ADC(28)

VREF = 3.3
CONV = VREF / 65535

# =========================
# Wi-Fi 設定
# =========================
SSID = "aterm-5d78db-g"
PASSWORD = "5be63f163242f"
PC_IP = "192.168.0.230"
#SSID = "Buffalo-2G-7A68"
#PASSWORD = "8r8r58e6sfw8u"
#PC_IP = "192.168.0.231"
PORT = 5005

wlan = network.WLAN(network.STA_IF)
wlan.ifconfig((
    "192.168.0.50",   # Pico のIP
    "255.255.255.0",
    "192.168.0.1",    # 形式上のGW（使わない）
    "8.8.8.8"
))

wlan.active(True)

if not wlan.isconnected():
    wlan.connect(SSID, PASSWORD)
    while not wlan.isconnected():
        time.sleep_ms(100)

# =========================
# UDP 設定
# =========================
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
DEST = (PC_IP, PORT)

# =========================
# 周期制御用
# =========================
PERIOD_MS = 10
next_tick = time.ticks_ms()

# =========================
# メインループ
# =========================
while True:
    now = time.ticks_ms()
    if time.ticks_diff(now, next_tick) >= 0:
        next_tick = time.ticks_add(next_tick, PERIOD_MS)

        # ADC 読み取り
        v0 = adc0.read_u16() * CONV
        v2 = adc2.read_u16() * CONV

        msg = f"{v0:.3f},{v2:.3f}\n"
        sock.sendto(msg.encode(), DEST)

        # デバッグは間引き推奨
        print(msg)
