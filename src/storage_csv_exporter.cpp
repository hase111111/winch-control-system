#include "storage_csv_exporter.hpp"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace winch {

StorageCsvExporter::StorageCsvExporter(
    const std::vector<std::pair<std::string, std::shared_ptr<TimeSeriesStorage>>>& storages)
    : storages_(storages) {}

bool StorageCsvExporter::ExportInteractive(const std::string& output_dir) const {
    if (!ConfirmExport()) {
        std::cout << "CSV出力はスキップしました。" << std::endl;
        return false;
    }

    const std::string dir_name = PromptBaseName();
    if (dir_name.empty()) {
        std::cerr << "ディレクトリ名が空のため、CSV出力を中止します。" << std::endl;
        return false;
    }

    const std::string safe_dir = SanitizeName(dir_name);
    const std::filesystem::path base_dir = std::filesystem::path(output_dir) / safe_dir;

    std::error_code ec;
    std::filesystem::create_directories(base_dir, ec);
    if (ec) {
        std::cerr << "出力ディレクトリの作成に失敗しました: " << base_dir.string() << std::endl;
        return false;
    }

    const std::string timestamp = MakeTimestampPrefix();

    size_t written_files = 0;
    for (const auto& [name, storage] : storages_) {
        if (!storage) {
            continue;
        }
        auto snapshot = storage->GetSnapshot();
        if (snapshot.empty()) {
            continue;
        }

        const std::string safe_name = SanitizeName(name);
        const std::string file_name = safe_name + "_" + timestamp + ".csv";
        const std::filesystem::path file_path = base_dir / file_name;

        std::ofstream ofs(file_path);
        if (!ofs.is_open()) {
            std::cerr << "CSVファイルを開けません: " << file_path.string() << std::endl;
            continue;
        }

        ofs << "time,value\n";
        for (const auto& [time, value] : snapshot) {
            ofs << time << "," << value << "\n";
        }
        ++written_files;
    }

    if (written_files == 0) {
        std::cout << "出力対象のデータがありませんでした。" << std::endl;
        return false;
    }

    std::cout << "CSV出力が完了しました。出力先: " << base_dir.string() << std::endl;
    return true;
}

bool StorageCsvExporter::ConfirmExport() const {
    std::string input;
    std::cout << "CSVを出力しますか? (y/n): ";
    std::getline(std::cin, input);

    std::transform(input.begin(), input.end(), input.begin(), ::tolower);
    if (input == "y" || input == "yes") {
        return true;
    }

    if (input == "n" || input == "no") {
        std::cout << "本当にCSV出力をスキップしますか? (y/n): ";
        std::getline(std::cin, input);
        std::transform(input.begin(), input.end(), input.begin(), ::tolower);
        return !(input == "y" || input == "yes");
    }

    return false;
}

std::string StorageCsvExporter::PromptBaseName() const {
    std::string base_name;
    std::cout << "保存先ディレクトリ名を入力してください: ";
    std::getline(std::cin, base_name);

    // 先頭末尾の空白を除去
    base_name.erase(0, base_name.find_first_not_of(" \t"));
    base_name.erase(base_name.find_last_not_of(" \t") + 1);

    return base_name;
}

std::string StorageCsvExporter::MakeTimestampPrefix() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t now_time = std::chrono::system_clock::to_time_t(now);

    std::tm tm_buf {};
#if defined(_WIN32)
    localtime_s(&tm_buf, &now_time);
#else
    localtime_r(&now_time, &tm_buf);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y%m%d_%H%M%S");
    return oss.str();
}

std::string StorageCsvExporter::SanitizeName(const std::string& name) {
    std::string result = name;
    for (auto& ch : result) {
        if (ch == ' ' || ch == '/' || ch == '\\' || ch == ':') {
            ch = '_';
        }
    }
    return result;
}

bool StorageCsvExporter::WriteStorageCsv(const std::string& file_path,
                                        const TimeSeriesStorage& storage) const {
    auto snapshot = storage.GetSnapshot();
    if (snapshot.empty()) {
        return false;
    }

    std::ofstream ofs(file_path);
    if (!ofs.is_open()) {
        return false;
    }

    ofs << "time,value\n";
    for (const auto& [time, value] : snapshot) {
        ofs << time << "," << value << "\n";
    }

    return true;
}

}  // namespace winch
