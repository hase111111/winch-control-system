#include <iostream>
#include <thread>
#include <memory>
#include <atomic>
#include <vector>
#include <utility>
#include <string>

#include "input_handler.hpp"
#include "time_series_storage.hpp"

int main(int argc, char* argv[]) {
    // 使用するクラスを using 宣言で指定．
    using winch::InputHandler;
    using winch::TimeSeriesStorage;

    std::atomic_bool stop_flag(false);

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
