#include <iostream>
#include <thread>
#include <memory>
#include <atomic>
#include <vector>
#include <string>

#include "config_loader.hpp"
#include "handler_factory.hpp"
#include "i_handler.hpp"
#include "storage_factory.hpp"
#include "time_series_storage.hpp"

int main(int argc, char* argv[]) {
    // 使用するクラスを using 宣言で指定．
    using winch::ConfigLoader;
    using winch::HandlerFactory;
    using winch::IHandler;
    using winch::StorageFactory;
    using winch::TimeSeriesStorage;

    // 全ハンドラーで共有する停止フラグ.
    std::atomic_bool stop_flag(false);

    // 設定ファイルを読み込み.
    const std::string config_path = (argc > 1) ? argv[1] : "config.ini";
    ConfigLoader config;
    if (!config.Load(config_path)) {
        std::cerr << "設定ファイルの読み込みに失敗しました．" << std::endl;
        return 1;
    }

    // データ保存用クラスを作成（名前付き）.
    auto storages = StorageFactory::CreateDefaultStorages();

    // ハンドラーファクトリで生成.
    HandlerFactory factory(config, stop_flag);
    std::vector<std::shared_ptr<IHandler>> handlers = factory.CreateHandlers(storages);

    for (const auto& handler : handlers) {
        if (!handler->Initialize()) {
            std::cerr << "ハンドラーの初期化に失敗しました．" << std::endl;
            return 1;
        }
    }

    // 別スレッドでUpdateを実行.
    std::vector<std::thread> threads;
    threads.reserve(handlers.size());
    for (const auto& handler : handlers) {
        threads.emplace_back([handler]() { handler->Update(); });
    }

    // メインスレッドでスレッド終了を待つ.
    for (auto& t : threads) {
        t.join();
    }

    // クリーンアップ.
    for (const auto& handler : handlers) {
        handler->Finalize();
    }

    return 0;
}
