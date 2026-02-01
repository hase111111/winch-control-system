#ifndef CAN_CONTROLLER_HANDLER_HPP
#define CAN_CONTROLLER_HANDLER_HPP

#include <atomic>
#include <string>
#include <cstdint>
#include <memory>

#include "i_handler.hpp"
#include "time_series_storage.hpp"
#include "config_loader.hpp"

namespace winch {

//! @brief ゲームパッドの入力を受け取り、CANでモータを制御するクラス.
class CanControllerHandler final : public IHandler {
public:
    CanControllerHandler(const ConfigLoader& config,
                        const std::atomic_bool& stop_flag,
                        std::shared_ptr<TimeSeriesStorage> motor0_control_storage,
                        std::shared_ptr<TimeSeriesStorage> motor1_control_storage,
                        std::shared_ptr<TimeSeriesStorage> motor0_encoder_storage,
                        std::shared_ptr<TimeSeriesStorage> motor1_encoder_storage);

    bool Initialize() override;
        
    //! @brief ゲームパッド入力を読み取りCANで速度指令を送信する.
    //! キャリブレーション→クローズドループ制御→コントローラー入力ループを実行.
    //! 内部でwhileループを回すため、別スレッドで実行すること.
    void Update() override;
    
    void Finalize() override;
    
private:
    //! @brief 指定したノードIDのモーターに軸状態を送信する.
    void SendAxisState(int node_id, uint32_t state);
    
    //! @brief 指定したノードIDのモーターに速度指令を送信する.
    void SendVelocity(int node_id, float vel_turn_s);
    
    //! @brief CANメッセージを受信してエンコーダ値を処理する.
    void ReceiveCanMessages(int timeout_ms);
    
    //! @brief ゲームパッドの軸値を正規化する（-1.0 ~ 1.0）.
    float NormalizeAxis(int value);

    std::string interface_name_;
    std::string gamepad_device_;
    const std::atomic_bool& stop_flag_;
    
    int can_socket_;
    int gamepad_fd_;
    
    std::shared_ptr<TimeSeriesStorage> motor0_control_storage_;
    std::shared_ptr<TimeSeriesStorage> motor1_control_storage_;
    std::shared_ptr<TimeSeriesStorage> motor0_encoder_storage_;
    std::shared_ptr<TimeSeriesStorage> motor1_encoder_storage_;
    
    // 制御パラメータ
    float max_velocity_;
    float deadzone_;
    float low_gain_;
    float high_gain_;
    bool move_motors_;
};

}  // namespace winch

#endif // CAN_CONTROLLER_HANDLER_HPP
