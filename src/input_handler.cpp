#include "input_handler.hpp"

#include <iostream>
#include <string>

namespace winch {

InputHandler::InputHandler(std::atomic_bool& stop_flag,
                           const std::vector<std::pair<std::string, std::shared_ptr<TimeSeriesStorage>>>& storages)
    : stop_flag_(stop_flag), storages_(storages) {}

bool InputHandler::Initialize() {
    std::cout << "標準入力監視を開始します。" << std::endl;
    PrintHelp();
    return true;
}

void InputHandler::PrintHelp() const {
    std::cout << "\n利用可能なコマンド:" << std::endl;
    std::cout << "  exit, quit - システムを終了" << std::endl;
    std::cout << "  data       - 各ストレージの最新データを表示" << std::endl;
    std::cout << "  count      - 各ストレージのデータ数を表示" << std::endl;
    std::cout << "  help       - このヘルプを表示" << std::endl;
    std::cout << std::endl;
}

void InputHandler::Update() {
    std::string input;
    
    while (!stop_flag_.load()) {
        std::getline(std::cin, input);
        
        if (input == "exit" || input == "quit") {
            std::cout << "終了コマンドを受信しました。システムを停止します..." << std::endl;
            stop_flag_.store(true);
            break;
        }
        
        if (input == "help") {
            PrintHelp();
            continue;
        }
        
        if (input == "data") {
            std::cout << "\n=== 最新データ ===" << std::endl;
            for (const auto& [name, storage] : storages_) {
                if (storage) {
                    auto snapshot = storage->GetSnapshot();
                    if (!snapshot.empty()) {
                        const auto& [time, value] = snapshot.back();
                        printf("%s: [%6.3f s] %10.4f\n", name.c_str(), time, value);
                    } else {
                        printf("%s: データなし\n", name.c_str());
                    }
                }
            }
            std::cout << std::endl;
            continue;
        }
        
        if (input == "count") {
            std::cout << "\n=== データ数 ===" << std::endl;
            for (const auto& [name, storage] : storages_) {
                if (storage) {
                    auto snapshot = storage->GetSnapshot();
                    printf("%s: %zu 個\n", name.c_str(), snapshot.size());
                }
            }
            std::cout << std::endl;
            continue;
        }
        
        // 未知コマンド
        if (!input.empty()) {
            std::cout << "エラー: 不明なコマンドです。 'help' を入力してコマンド一覧を確認してください。" << std::endl;
        }
    }
}

void InputHandler::Finalize() {
    std::cout << "標準入力監視を終了しました。" << std::endl;
}

}  // namespace winch
