#ifndef CAN_HANDLER_HPP
#define CAN_HANDLER_HPP

#include <atomic>
#include <string>
#include <cstdint>
#include <memory>

#include "i_handler.hpp"
#include "time_series_storage.hpp"
#include "pd_controller.hpp"
#include "config_loader.hpp"

namespace winch {

class CanHandler final : public IHandler {
public:
    CanHandler(const ConfigLoader& config,
               const std::atomic_bool& stop_flag,
               std::shared_ptr<TimeSeriesStorage> roadcell_storage,
               std::shared_ptr<TimeSeriesStorage> potentiometer0_storage,
               std::shared_ptr<TimeSeriesStorage> potentiometer1_storage,
               std::shared_ptr<TimeSeriesStorage> motor0_control_storage,
               std::shared_ptr<TimeSeriesStorage> motor1_control_storage,
               std::shared_ptr<TimeSeriesStorage> motor0_encoder_storage,
               std::shared_ptr<TimeSeriesStorage> motor1_encoder_storage);

    bool Initialize() override;
        
    //! @brief CAN通信の更新処理を行う.
    //! キャリブレーション→10秒待機→速度指令ループを実行.
    //! 内部でwhileループを回すため、別スレッドで実行すること.
    void Update() override;
    
    void Finalize() override;
    
private:
    //! @brief 指定したノードIDのモーターに軸状態を送信する.
    //! @param node_id ノードID (1, 2, ...)
    //! @param state 軸状態 (例: 3=キャリブレーション, 8=クローズドループ制御)
    void SendAxisState(int node_id, uint32_t state);
    
    //! @brief 指定したノードIDのモーターに速度指令を送信する.
    //! @param node_id ノードID (1, 2, ...)
    //! @param vel_turn_s 目標速度 [回転/秒]
    void SendVelocity(int node_id, float vel_turn_s);
    
    //! @brief CANメッセージを受信してエンコーダ値を処理する.
    //! @param timeout_ms タイムアウト時間[ミリ秒]
    void ReceiveCanMessages(int timeout_ms);

    std::string interface_name_;
    const std::atomic_bool& stop_flag_;
    int can_socket_;
    
    std::shared_ptr<TimeSeriesStorage> roadcell_storage_;
    std::shared_ptr<TimeSeriesStorage> potentiometer0_storage_;
    std::shared_ptr<TimeSeriesStorage> potentiometer1_storage_;
    std::shared_ptr<TimeSeriesStorage> motor0_control_storage_;
    std::shared_ptr<TimeSeriesStorage> motor1_control_storage_;
    std::shared_ptr<TimeSeriesStorage> motor0_encoder_storage_;
    std::shared_ptr<TimeSeriesStorage> motor1_encoder_storage_;
    PDController pd_controller_motor1_{};
    PDController pd_controller_motor2_{};
    bool move_motors_;
    double gravity_compensation_;
};

}  // namespace winch

#endif // CAN_HANDLER_HPP
