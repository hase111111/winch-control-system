#include <iostream>
#include <thread>
#include <chrono>
#include <memory>
#include <atomic>
#include <string>

#include "serial_port_handler.hpp"
#include "time_series_storage.hpp"
#include "udp_handler.hpp"

int main(int argc, char* argv[]) {
    // 使用するクラスを using 宣言で指定．
    using winch::SerialPortHandler;
    using winch::TimeSeriesStorage;
    using winch::UdpHandler;

    std::atomic_bool stop_flag(false);
    const std::string port_name = "/dev/ttyUSB0";
    const int udp_port = 5005;

    // データ保存用クラスを作成
    auto storage = std::make_shared<TimeSeriesStorage>();

    // シリアルポートハンドラーを作成
    SerialPortHandler serial_port_handler(port_name, stop_flag, storage);

    if (!serial_port_handler.Initialize()) {
        std::cerr << "シリアルポートの初期化に失敗しました．" << std::endl;
        return 1;
    }

    // 別スレッドでUpdateを実行
    std::thread update_thread(&SerialPortHandler::Update, &serial_port_handler);

    // メインスレッドで10秒待機
    std::cout << "Running for 10 seconds..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(10));

    // 停止フラグを立てる
    std::cout << "Stopping..." << std::endl;
    stop_flag = true;

    // スレッドの終了を待つ
    update_thread.join();

    // 保存されたデータを出力
    auto data = storage->GetSnapshot();
    std::cout << "Collected " << data.size() << " data points:" << std::endl;
    for (const auto& [time, value] : data) {
        printf("[%6.3f s] %8.4f kN\n", time, value);
    }

    // クリーンアップ
    serial_port_handler.Finalize();

    return 0;
}
