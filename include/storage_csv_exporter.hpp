#ifndef STORAGE_CSV_EXPORTER_HPP
#define STORAGE_CSV_EXPORTER_HPP

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "time_series_storage.hpp"

namespace winch {

//! @brief TimeSeriesStorageの内容をCSVに出力するクラス.
class StorageCsvExporter final {
public:
    explicit StorageCsvExporter(
        const std::vector<std::pair<std::string, std::shared_ptr<TimeSeriesStorage>>>& storages);

    //! @brief 標準入力で確認し、CSVを出力する.
    //! @param output_dir 出力先ディレクトリ
    //! @return 出力した場合true、スキップした場合false
    bool ExportInteractive(const std::string& output_dir) const;

private:
    bool ConfirmExport() const;
    std::string PromptBaseName() const;

    static std::string MakeTimestampPrefix();
    static std::string SanitizeName(const std::string& name);

    bool WriteStorageCsv(const std::string& file_path,
                         const TimeSeriesStorage& storage) const;

    const std::vector<std::pair<std::string, std::shared_ptr<TimeSeriesStorage>>>& storages_;
};

}  // namespace winch

#endif // STORAGE_CSV_EXPORTER_HPP
