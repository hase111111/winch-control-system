#include "can_handler.hpp"

#include <iostream>
#include <cstring>
#include <thread>
#include <chrono>

#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/can.h>
#include <linux/can/raw.h>

namespace winch {

// https://docs.odriverobotics.com/v/latest/manual/can-protocol.html

constexpr int INVALID_SOCKET = -1;

// ODrive CAN コマンド（送信用）
constexpr uint16_t CMD_SET_AXIS_REQUESTED_STATE = 0x007;
constexpr uint16_t CMD_SET_INPUT_POS = 0x00C;
constexpr uint16_t CMD_SET_INPUT_VEL = 0x00D;

// ODrive 軸状態
constexpr uint32_t AXIS_STATE_IDLE = 1;
constexpr uint32_t AXIS_STATE_FULL_CALIBRATION_SEQUENCE = 3;
constexpr uint32_t AXIS_STATE_CLOSED_LOOP_CONTROL = 8;

CanHandler::CanHandler(const std::string& interface_name,
                       const std::atomic_bool& stop_flag,
                       std::shared_ptr<TimeSeriesStorage> roadcell_storage,
                       std::shared_ptr<TimeSeriesStorage> potentiometer_storage,
                       double kp1,
                       double kd1,
                       double kp2,
                       double kd2,
                       bool move_motors)
    : interface_name_(interface_name), stop_flag_(stop_flag),
      can_socket_(INVALID_SOCKET),
      roadcell_storage_(roadcell_storage),
      potentiometer_storage_(potentiometer_storage),
      pd_controller_motor1_(),
      pd_controller_motor2_(),
      move_motors_(move_motors) {
    // PD制御のゲインをconfigから設定
    pd_controller_motor1_.SetGains(kp1, kd1);
    pd_controller_motor2_.SetGains(kp2, kd2);
    
    std::cout << "モータ移動フラグ: " << (move_motors_ ? "有効" : "無効") << std::endl;
}

bool CanHandler::Initialize() {
    // CANソケットを作成
    can_socket_ = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (can_socket_ < 0) {
        std::cerr << "CANソケット作成に失敗しました．" << std::endl;
        return false;
    }

    struct ifreq ifr {};
    std::strncpy(ifr.ifr_name, interface_name_.c_str(), IFNAMSIZ - 1);
    if (ioctl(can_socket_, SIOCGIFINDEX, &ifr) < 0) {
        std::cerr << "CANインターフェース " << interface_name_ 
                  << " が見つかりません．" << std::endl;
        close(can_socket_);
        can_socket_ = INVALID_SOCKET;
        return false;
    }

    struct sockaddr_can addr {};
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(can_socket_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "CANソケットのバインドに失敗しました．" << std::endl;
        close(can_socket_);
        can_socket_ = INVALID_SOCKET;
        return false;
    }

    std::cout << "CANの初期化に成功しました．" << std::endl;

    return true;
}

void CanHandler::SendAxisState(const int node_id, const uint32_t state) {
    if (can_socket_ < 0) return;

    struct can_frame frame {};
    frame.can_id = (node_id << 5) | CMD_SET_AXIS_REQUESTED_STATE;
    frame.can_dlc = 4;
    std::memcpy(frame.data, &state, 4);
    write(can_socket_, &frame, sizeof(frame));
}

void CanHandler::SendVelocity(const int node_id, const float vel_turn_s) {
    if (can_socket_ < 0) return;

    struct can_frame frame {};
    frame.can_id = (node_id << 5) | CMD_SET_INPUT_VEL;
    frame.can_dlc = 4;
    std::memcpy(frame.data, &vel_turn_s, 4);
    write(can_socket_, &frame, sizeof(frame));
}

void CanHandler::Update() {
    if (can_socket_ < 0) return;

    // キャリブレーション開始.
    SendAxisState(1, AXIS_STATE_FULL_CALIBRATION_SEQUENCE);
    SendAxisState(2, AXIS_STATE_FULL_CALIBRATION_SEQUENCE);

    // 10秒待機.
    std::this_thread::sleep_for(std::chrono::seconds(10));

    if (stop_flag_.load()) return;

    // クローズドループ制御に移行.
    SendAxisState(1, AXIS_STATE_CLOSED_LOOP_CONTROL);
    SendAxisState(2, AXIS_STATE_CLOSED_LOOP_CONTROL);

    std::this_thread::sleep_for(std::chrono::seconds(1));

    // PD制御ループ (100Hz).
    constexpr int hz = 100;
    auto next_send_time = std::chrono::steady_clock::now();

    while (!stop_flag_.load()) {
        auto now = std::chrono::steady_clock::now();
        if (now < next_send_time) {
            std::this_thread::sleep_for(next_send_time - now);
        }
        next_send_time += std::chrono::milliseconds(1000 / hz);

        // 目標値（Roadcell）と現在値（Potentiometer）を取得
        double target = roadcell_storage_->GetLatestValue();
        double current = potentiometer_storage_->GetLatestValue();
        double d_error_dt = potentiometer_storage_->GetLatestDifference();
        
        // PD制御で誤差を計算
        double error = target - current;
        double control_output_motor1 = pd_controller_motor1_.Compute(error, d_error_dt);
        double control_output_motor2 = pd_controller_motor2_.Compute(error, d_error_dt);
        
        // 制御出力をモータ速度指令に変換（適宜スケーリングが必要）
        float velocity_cmd_motor1 = static_cast<float>(control_output_motor1);
        float velocity_cmd_motor2 = static_cast<float>(control_output_motor2);
        
        // move_motors_がtrueの場合のみ速度指令を送信
        if (move_motors_) {
            SendVelocity(1, velocity_cmd_motor1);
            SendVelocity(2, velocity_cmd_motor2);
        }
    }
}

void CanHandler::Finalize() {
    if (can_socket_ >= 0) {
        // モータを停止させる.
        SendVelocity(1, 0.0f);
        SendVelocity(2, 0.0f);
        
        // 少し待機してから軸をIDLE状態に.
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        SendAxisState(1, AXIS_STATE_IDLE);
        SendAxisState(2, AXIS_STATE_IDLE);
        
        // さらに少し待機してコマンドが送信されるのを確認.
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // ソケットを閉じる.
        close(can_socket_);
        can_socket_ = INVALID_SOCKET;
    }
}

}  // namespace winch
