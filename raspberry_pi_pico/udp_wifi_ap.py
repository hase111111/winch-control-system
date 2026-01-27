import socket

PORT = 5005
PC_IP = "192.168.0.169"   # 自分のPCのIPアドレスに合わせる

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((PC_IP, PORT))

print("UDP receiver started on", PC_IP)

while True:
    data, addr = sock.recvfrom(1024)
    print("RX:", addr, data.decode().strip())
