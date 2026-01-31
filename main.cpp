#include <iostream>
#include <thread>
#include <memory>
#include <atomic>
#include <vector>
#include <utility>
#include <string>

#include "config_loader.hpp"
#include "input_handler.hpp"
#include "time_series_storage.hpp"

int main(int argc, char* argv[]) {
    // 使用するクラスを using 宣言で指定．
    using winch::ConfigLoader;
    using winch::InputHandler;
    using winch::TimeSeriesStorage;

    std::atomic_bool stop_flag(false);

    // 設定ファイルを読み込み
    const std::string config_path = (argc > 1) ? argv[1] : "config.ini";
    ConfigLoader config;
    if (!config.Load(config_path)) {
        std::cerr << "設定ファイルの読み込みに失敗しました．" << std::endl;
    }

    const std::string serial_port = config.GetVal<std::string>("Serial", "port", "/dev/ttyUSB0");
    const int udp_port = config.GetVal<int>("UDP", "port", 5005);
    std::cout << "Serial port: " << serial_port << std::endl;
    std::cout << "UDP port: " << udp_port << std::endl;

    // データ保存用クラスを作成（名前付き）
    std::vector<std::pair<std::string, std::shared_ptr<TimeSeriesStorage>>> storages = {
        {"Serial Port", std::make_shared<TimeSeriesStorage>()},
        {"UDP Potentiometer", std::make_shared<TimeSeriesStorage>()},
        {"CAN Motor", std::make_shared<TimeSeriesStorage>()},
    };

    // 入力ハンドラーを作成
    InputHandler input_handler(stop_flag, storages);

    if (!input_handler.Initialize()) {
        std::cerr << "入力ハンドラーの初期化に失敗しました．" << std::endl;
        return 1;
    }

    // 別スレッドでUpdateを実行
    std::thread input_thread(&InputHandler::Update, &input_handler);

    // メインスレッドでスレッド終了を待つ
    input_thread.join();

    // クリーンアップ
    input_handler.Finalize();

    return 0;
}
