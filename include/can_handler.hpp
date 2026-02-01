#ifndef CAN_HANDLER_HPP
#define CAN_HANDLER_HPP

#include <atomic>
#include <string>
#include <cstdint>
#include <memory>

#include "i_handler.hpp"
#include "time_series_storage.hpp"
#include "pd_controller.hpp"

namespace winch {

class CanHandler final : public IHandler {
public:
    CanHandler(const std::string& interface_name,
               const std::atomic_bool& stop_flag,
               std::shared_ptr<TimeSeriesStorage> roadcell_storage,
               std::shared_ptr<TimeSeriesStorage> potentiometer_storage,
               double kp1,
               double kd1,
               double kp2,
               double kd2,
               bool move_motors);

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

    std::string interface_name_;
    const std::atomic_bool& stop_flag_;
    int can_socket_;
    
    std::shared_ptr<TimeSeriesStorage> roadcell_storage_;
    std::shared_ptr<TimeSeriesStorage> potentiometer_storage_;
    PDController pd_controller_motor1_;
    PDController pd_controller_motor2_;
    bool move_motors_;
};

}  // namespace winch

#endif // CAN_HANDLER_HPP
