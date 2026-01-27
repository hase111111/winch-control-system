#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <cstring>
#include <chrono>

// ===== Serial =====
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <unistd.h>

// ===== UDP =====
#include <arpa/inet.h>
#include <sys/socket.h>

#define SERIAL_PORT "/dev/ttyUSB0"
#define UDP_PORT 5005

// ==========================
// 共有データ
// ==========================
std::mutex data_mutex;
double loadcell = 0.0;
double pot1 = 0.0;
double pot2 = 0.0;
std::atomic<bool> running(true);

// ==========================
// シリアル設定
// ==========================
int serial_port;

bool setupSerial() {
    serial_port = open(SERIAL_PORT, O_RDWR | O_NOCTTY);
    if (serial_port < 0) {
        perror("open serial");
        return false;
    }

    termios tty{};
    tcgetattr(serial_port, &tty);

    cfsetispeed(&tty, B115200);
    cfsetospeed(&tty, B115200);

    tty.c_cflag = CS8 | CREAD | CLOCAL;
    tty.c_cflag |= PARENB;
    tty.c_cflag &= ~PARODD;
    tty.c_cflag &= ~CSTOPB;

    tty.c_lflag = 0;
    tty.c_iflag = 0;
    tty.c_oflag = 0;

    tty.c_cc[VTIME] = 10;
    tty.c_cc[VMIN] = 0;

    tcsetattr(serial_port, TCSANOW, &tty);
    return true;
}

// ==========================
// シリアル受信スレッド
// ==========================
void serialThread() {
    std::string buffer;
    char c;

    while (running) {
        int n = read(serial_port, &c, 1);
        if (n <= 0) continue;

        if (c == '\n') {
            // 例: DSCB01,+12.345
            auto pos = buffer.find(',');
            if (pos != std::string::npos) {
                try {
                    double value = std::stod(buffer.substr(pos + 1));
                    std::lock_guard<std::mutex> lock(data_mutex);
                    loadcell = value;
                } catch (...) {}
            }
            buffer.clear();
        } else if (c != '\r') {
            buffer += c;
        }
    }
}

// ==========================
// UDP受信スレッド
// ==========================
void udpThread() {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(UDP_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(sock, (sockaddr*)&addr, sizeof(addr));

    char buf[256];

    while (running) {
        ssize_t len = recv(sock, buf, sizeof(buf) - 1, 0);
        if (len <= 0) continue;

        buf[len] = '\0';
        // 例: 1.234,2.876
        try {
            std::string s(buf);
            auto p = s.find(',');
            if (p != std::string::npos) {
                double v1 = std::stod(s.substr(0, p));
                double v2 = std::stod(s.substr(p + 1));
                std::lock_guard<std::mutex> lock(data_mutex);
                pot1 = v1;
                pot2 = v2;
            }
        } catch (...) {}
    }

    close(sock);
}

// ==========================
// 出力スレッド
// ==========================
void outputThread() {
    using namespace std::chrono;
    while (running) {
        {
            std::lock_guard<std::mutex> lock(data_mutex);
            printf("%8.4f,%6.3f,%6.3f\n", loadcell, pot1, pot2);
            fflush(stdout);
        }
        std::this_thread::sleep_for(milliseconds(10)); // 100 Hz
    }
}

// ==========================
// main
// ==========================
int main() {
    if (!setupSerial()) return 1;

    // ロードセル連続送信開始
    write(serial_port, "NO99CX\r", 7);

    std::thread th_serial(serialThread);
    std::thread th_udp(udpThread);
    std::thread th_out(outputThread);

    std::cout << "Running... Press Ctrl+C to stop\n";
    std::this_thread::sleep_for(std::chrono::seconds(10));

    running = false;

    th_serial.join();
    th_udp.join();
    th_out.join();

    write(serial_port, "NO99CY\r", 7);
    close(serial_port);

    return 0;
}
