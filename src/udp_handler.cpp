#include "udp_handler.hpp"

#include <iostream>
#include <cstring>
#include <chrono>
#include <thread>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

namespace winch {

constexpr int INVALID_SOCKET = -1;

UdpHandler::UdpHandler(const int port,
                       const std::atomic_bool& stop_flag,
                       const std::shared_ptr<TimeSeriesStorage>& pot1_storage,
                       const std::shared_ptr<TimeSeriesStorage>& pot2_storage)
    : port_(port), stop_flag_(stop_flag),
      pot1_storage_(pot1_storage),
      pot2_storage_(pot2_storage),
      socket_(INVALID_SOCKET) {}

bool UdpHandler::Initialize() {
    // UDPソケットを作成
    socket_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_ < 0) {
        std::cerr << "UDPのソケット作成に失敗しました．" << std::endl;
        return false;
    }

    struct sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(socket_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "ポート " << port_ << " でUDPソケットのバインドに失敗しました．" << std::endl;
        close(socket_);
        socket_ = INVALID_SOCKET;
        return false;
    }

    return true;
}

void UdpHandler::Update() {
    if (socket_ < 0) return;

    const auto start_time = std::chrono::steady_clock::now();
    constexpr int kHz100_ms = 10;  // 100 Hz = 10 ms
    auto next_read_time = std::chrono::steady_clock::now();

    char buffer[256];
    struct sockaddr_in src_addr {};
    socklen_t src_len = sizeof(src_addr);

    while (!stop_flag_.load()) {
        // 100Hz周期でポーリング.
        auto now = std::chrono::steady_clock::now();
        if (now < next_read_time) {
            std::this_thread::sleep_for(next_read_time - now);
        }
        next_read_time += std::chrono::milliseconds(kHz100_ms);

        // ノンブロッキングで受信.
        ssize_t len = recvfrom(socket_, buffer, sizeof(buffer) - 1, MSG_DONTWAIT,
                               (struct sockaddr*)&src_addr, &src_len);

        if (len > 0) {
            buffer[len] = '\0';
            // 例: 1.234,2.876.
            try {
                std::string data(buffer);
                size_t comma_pos = data.find(',');
                if (comma_pos != std::string::npos) {
                    double pot1 = std::stod(data.substr(0, comma_pos));
                    double pot2 = std::stod(data.substr(comma_pos + 1));

                    auto now = std::chrono::steady_clock::now();
                    std::chrono::duration<double> elapsed = now - start_time;
                    double time_sec = elapsed.count();

                    if (pot1_storage_) {
                        pot1_storage_->Add(time_sec, pot1);
                    }
                    if (pot2_storage_) {
                        pot2_storage_->Add(time_sec, pot2);
                    }
                }
            }
            catch (...) {
                // パース失敗は無視.
            }
        }
        // len <= 0 の場合はデータなし。次の周期まで待機.
    }
}

void UdpHandler::Finalize() {
    if (socket_ >= 0) {
        close(socket_);
        socket_ = INVALID_SOCKET;
    }
}

}  // namespace winch
