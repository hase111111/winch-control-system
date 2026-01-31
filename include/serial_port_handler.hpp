
#ifndef SERIAL_PORT_HANDLER_HPP
#define SERIAL_PORT_HANDLER_HPP

#include <atomic>
#include <memory>
#include <string>

#include "time_series_storage.hpp"

namespace winch {

//! @brief シリアルポート通信を扱うクラス.
//! ユニパルス製のロードセルDSCB-50kNとの通信を想定している. 
class SerialPortHandler final {
public:
    SerialPortHandler(const std::string& port_name,
                      const std::atomic_bool& stop_flag,
                      std::shared_ptr<TimeSeriesStorage> storage);

    //! @brief シリアルポート通信の初期化を行う.
    //! DSCB-50kNとの通信用に設定をしているため，
    //! 他の機器と通信する場合は適宜修正が必要.
    //! @return 初期化に成功したらtrue，失敗したらfalseを返す.
    bool Initialize();

    //! @brief シリアルポート通信の更新処理を行う.
    //! 内部でwhileループを回すため，別スレッドで実行すること.
    //! コンストラクタでこのクラスに渡すフラグを監視し，
    //! 終了フラグが立ったらループを抜ける.
    void Update();

    void Finalize();

private:
    const std::string port_name_;
    const std::atomic_bool& stop_flag_;
    const std::shared_ptr<TimeSeriesStorage> storage_;
    
    int serial_port_;  //!< この値はInitializeで設定するので非const.
    std::atomic_bool is_initialized_{false};  //!< 初期化済みフラグ.
};

}  // namespace winch

#endif // SERIAL_PORT_HANDLER_HPP
