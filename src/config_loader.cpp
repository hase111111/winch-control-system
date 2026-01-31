#include "config_loader.hpp"

#include <iostream>
#include <fstream>

namespace winch {

ConfigLoader::ConfigLoader() {}

bool ConfigLoader::Load(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "設定ファイル " << filename << " を開くことができません．" << std::endl;
        return false;
    }

    std::string line;
    std::string current_section;

    while (std::getline(file, line)) {
        // コメント行と空行をスキップ
        if (line.empty() || line[0] == ';' || line[0] == '#') {
            continue;
        }

        // 行の前後の空白を削除
        line.erase(0, line.find_first_not_of(" \t"));
        line.erase(line.find_last_not_of(" \t") + 1);

        // セクション行 [Section]
        if (line[0] == '[' && line[line.length() - 1] == ']') {
            current_section = line.substr(1, line.length() - 2);
            continue;
        }

        // キー=値行
        size_t eq_pos = line.find('=');
        if (eq_pos != std::string::npos && !current_section.empty()) {
            std::string key = line.substr(0, eq_pos);
            std::string value = line.substr(eq_pos + 1);

            // 前後の空白を削除
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);

            Set(current_section, key, value);
        }
    }

    file.close();

    return true;
}

void ConfigLoader::Set(const std::string& section, const std::string& key,
                       const std::string& value) {
    std::string full_key = section + "." + key;
    config_[full_key] = value;
}

}  // namespace winch
