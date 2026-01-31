#ifndef CONFIG_LOADER_HPP
#define CONFIG_LOADER_HPP

#include <string>
#include <map>
#include <algorithm>

namespace winch {

//! @brief 設定ファイルから設定値を読み込むクラス.
//! INI形式の設定ファイルをサポート.
class ConfigLoader {
public:
    //! @brief デフォルトコンストラクタ.
    ConfigLoader();

    //! @brief 設定ファイルを読み込む.
    //! @param filename 設定ファイルのパス.
    //! @return 読み込みに成功したらtrue、失敗したらfalseを返す.
    bool Load(const std::string& filename);

    //! @brief 設定値を取得する（テンプレート関数）.
    //! @tparam T 取得する値の型.
    //! @param section セクション名 (e.g., "Serial", "UDP").
    //! @param key キー名 (e.g., "port").
    //! @param default_value デフォルト値.
    //! @return 設定値、見つからない場合はデフォルト値を返す.
    template <typename T>
    T GetVal(const std::string& section, const std::string& key,
             const T& default_value = T{}) const {
        std::string full_key = section + "." + key;
        auto it = config_.find(full_key);
        if (it == config_.end()) {
            return default_value;
        }

        try {
            return ConvertValue<T>(it->second);
        } catch (...) {
            return default_value;
        }
    }

private:
    //! @brief 文字列から型Tへの変換を行う（テンプレート特殊化）.
    template <typename T>
    static T ConvertValue(const std::string& str);

    //! @brief 設定値を内部マップに格納する.
    //! @param section セクション名.
    //! @param key キー名.
    //! @param value 値.
    void Set(const std::string& section, const std::string& key,
             const std::string& value);

    // 設定を格納するマップ: section.key -> value.
    std::map<std::string, std::string> config_;
};

// テンプレート特殊化: std::string.
template <>
inline std::string ConfigLoader::ConvertValue<std::string>(const std::string& str) {
    return str;
}

// テンプレート特殊化: int.
template <>
inline int ConfigLoader::ConvertValue<int>(const std::string& str) {
    return std::stoi(str);
}

// テンプレート特殊化: double.
template <>
inline double ConfigLoader::ConvertValue<double>(const std::string& str) {
    return std::stod(str);
}

// テンプレート特殊化: bool.
template <>
inline bool ConfigLoader::ConvertValue<bool>(const std::string& str) {
    std::string lower_str = str;
    std::transform(lower_str.begin(), lower_str.end(), lower_str.begin(), ::tolower);
    return (lower_str == "true" || lower_str == "yes" || lower_str == "1");
}

}  // namespace winch

#endif // CONFIG_LOADER_HPP
