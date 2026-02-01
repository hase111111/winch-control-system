#ifndef HANDLER_FACTORY_HPP
#define HANDLER_FACTORY_HPP

#include <atomic>
#include <memory>
#include <vector>

#include "time_series_storage.hpp"

#include "config_loader.hpp"
#include "i_handler.hpp"

namespace winch {

//! @brief ハンドラーを生成するファクトリクラス.
class HandlerFactory final {
public:
    HandlerFactory(const ConfigLoader& config, std::atomic_bool& stop_flag);

    //! @brief ハンドラーを生成して返す.
    //! @param storages データ保存用ストレージの(name, storage)ペアvector.
    //! @return 生成したハンドラーのvector.
    std::vector<std::shared_ptr<IHandler>> CreateHandlers(
        const std::vector<std::pair<std::string, std::shared_ptr<TimeSeriesStorage>>>& storages) const;

private:
    //! @brief シリアルハンドラーを生成する.
    std::shared_ptr<IHandler> CreateSerialHandler(
        const std::vector<std::pair<std::string, std::shared_ptr<TimeSeriesStorage>>>& storages) const;

    //! @brief UDPハンドラーを生成する.
    std::shared_ptr<IHandler> CreateUdpHandler(
        const std::vector<std::pair<std::string, std::shared_ptr<TimeSeriesStorage>>>& storages) const;

    //! @brief CANハンドラーを生成する.
    std::shared_ptr<IHandler> CreateCanHandler(
        const std::vector<std::pair<std::string, std::shared_ptr<TimeSeriesStorage>>>& storages) const;
    
    //! @brief CANコントローラーハンドラーを生成する.
    std::shared_ptr<IHandler> CreateCanControllerHandler(
        const std::vector<std::pair<std::string, std::shared_ptr<TimeSeriesStorage>>>& storages) const;

    //! @brief 名前でストレージを検索する.
    //! @param storages データ保存用ストレージの(name, storage)ペアvector.
    //! @param name 検索するストレージ名.
    //! @return 見つかったストレージのshared_ptr（見つからない場合はnullptr）.
    std::shared_ptr<TimeSeriesStorage> FindStorageByName(
        const std::vector<std::pair<std::string, std::shared_ptr<TimeSeriesStorage>>>& storages,
        const std::string& name) const;

    const ConfigLoader& config_;
    std::atomic_bool& stop_flag_;
};

}  // namespace winch

#endif // HANDLER_FACTORY_HPP
