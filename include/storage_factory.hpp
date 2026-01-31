#ifndef STORAGE_FACTORY_HPP
#define STORAGE_FACTORY_HPP

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "time_series_storage.hpp"

namespace winch {

//! @brief データ保存用ストレージを生成するクラス.
class StorageFactory {
public:
    //! @brief デフォルトのストレージ一覧を生成する.
    static std::vector<std::pair<std::string, std::shared_ptr<TimeSeriesStorage>>> CreateDefaultStorages();
};

}  // namespace winch

#endif // STORAGE_FACTORY_HPP
