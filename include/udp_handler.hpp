#ifndef UDP_HANDLER_HPP
#define UDP_HANDLER_HPP

#include <atomic>
#include <memory>

#include "i_handler.hpp"
#include "config_loader.hpp"
#include "time_series_storage.hpp"

namespace winch {

//! @brief UDP通信を扱うクラス.
//! 本装置ではRaspberry Pi Pico2Wからの2台のポテンショメータ値受信を想定している.
//! 受信したポテンショメータ値はそれぞれTimeSeriesStorageに保存される.
class UdpHandler final : public IHandler {
public:
    UdpHandler(int port,
               const std::atomic_bool& stop_flag,
               const ConfigLoader& config,
               const std::shared_ptr<TimeSeriesStorage>& pot1_storage,
               const std::shared_ptr<TimeSeriesStorage>& pot2_storage);

    //! @brief UDP通信の初期化を行う.
    //! @return 初期化に成功したらtrue，失敗したらfalseを返す.
    bool Initialize() override;
    
    //! @brief UDP通信の更新処理を行う.
    //! 内部でwhileループを回すため，別スレッドで実行すること.
    //! コンストラクタでこのクラスに渡すフラグを監視し，
    //! 終了フラグが立ったらループを抜ける.
    void Update() override;
    
    void Finalize() override;
    
private:
    const int port_;
    const double offset_degree0_;
    const double offset_degree1_;
    const std::atomic_bool& stop_flag_;
    const std::shared_ptr<TimeSeriesStorage> pot1_storage_;
    const std::shared_ptr<TimeSeriesStorage> pot2_storage_;

    int socket_;
};

}  // namespace winch

#endif // UDP_HANDLER_HPP
