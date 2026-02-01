#include "can_handler.hpp"

#include <iostream>
#include <cstring>
#include <thread>
#include <chrono>

#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/can.h>
#include <linux/can/raw.h>

namespace winch {

// https://docs.odriverobotics.com/v/latest/manual/can-protocol.html

namespace {

constexpr int INVALID_SOCKET = -1;

// ODrive CAN コマンド（送信用）
constexpr uint16_t CMD_SET_AXIS_REQUESTED_STATE = 0x007;
constexpr uint16_t CMD_GET_ENCODER_ESTIMATES = 0x009;
constexpr uint16_t CMD_SET_INPUT_POS = 0x00C;
constexpr uint16_t CMD_SET_INPUT_VEL = 0x00D;

// ODrive 軸状態
constexpr uint32_t AXIS_STATE_IDLE = 1;
constexpr uint32_t AXIS_STATE_FULL_CALIBRATION_SEQUENCE = 3;
constexpr uint32_t AXIS_STATE_CLOSED_LOOP_CONTROL = 8;

// 重力加速度 [m/s^2].
constexpr double GRAVITY_ACCELERATION = 9.80665;

// 1 / (2 * cos(60 degrees)) = 1 / (2 * 0.5) = 1.0
const double P_TO_T = 1.0;

}  // namespace

CanHandler::CanHandler(const ConfigLoader& config,
                       const std::atomic_bool& stop_flag,
                       std::shared_ptr<TimeSeriesStorage> roadcell_storage,
                       std::shared_ptr<TimeSeriesStorage> potentiometer_storage,
                       std::shared_ptr<TimeSeriesStorage> motor0_control_storage,
                       std::shared_ptr<TimeSeriesStorage> motor1_control_storage,
                       std::shared_ptr<TimeSeriesStorage> motor0_encoder_storage,
                       std::shared_ptr<TimeSeriesStorage> motor1_encoder_storage)
    : interface_name_(config.GetVal<std::string>("CAN", "interface", "can0")),
      stop_flag_(stop_flag),
      can_socket_(INVALID_SOCKET),
      roadcell_storage_(roadcell_storage),
      potentiometer_storage_(potentiometer_storage),
      motor0_control_storage_(motor0_control_storage),
      motor1_control_storage_(motor1_control_storage),
      motor0_encoder_storage_(motor0_encoder_storage),
      motor1_encoder_storage_(motor1_encoder_storage),
      move_motors_(config.GetVal<bool>("Flags", "move_motors")),
      gravity_compensation_(config.GetVal<double>("PDControl", "gravity_compensation_kg")) {
    // PD制御のゲインをconfigから設定.
    const double kp1 = config.GetVal<double>("PDControl", "motor1_kp");
    const double kd1 = config.GetVal<double>("PDControl", "motor1_kd");
    const double kp2 = config.GetVal<double>("PDControl", "motor2_kp");
    const double kd2 = config.GetVal<double>("PDControl", "motor2_kd");
    
    // PDコントローラにゲインを設定.
    pd_controller_motor1_.SetGains(kp1, kd1);
    pd_controller_motor2_.SetGains(kp2, kd2);
    
    // 設定内容を表示.
    std::cout << "CAN interface: " << interface_name_ << std::endl;
    std::cout << "PD Gains Motor1: Kp=" << kp1 << ", Kd=" << kd1 << std::endl;
    std::cout << "PD Gains Motor2: Kp=" << kp2 << ", Kd=" << kd2 << std::endl;
    std::cout << "モータ移動フラグ: " << (move_motors_ ? "有効" : "無効") << std::endl;
    std::cout << "重力補償値: " << gravity_compensation_ << " kg" << std::endl;
}

bool CanHandler::Initialize() {
    // CANソケットを作成
    can_socket_ = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (can_socket_ < 0) {
        std::cerr << "CANソケット作成に失敗しました." << std::endl;
        return false;
    }

    struct ifreq ifr {};
    std::strncpy(ifr.ifr_name, interface_name_.c_str(), IFNAMSIZ - 1);
    if (ioctl(can_socket_, SIOCGIFINDEX, &ifr) < 0) {
        std::cerr << "CANインターフェース " << interface_name_ 
                  << " が見つかりません." << std::endl;
        close(can_socket_);
        can_socket_ = INVALID_SOCKET;
        return false;
    }

    struct sockaddr_can addr {};
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(can_socket_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "CANソケットのバインドに失敗しました." << std::endl;
        close(can_socket_);
        can_socket_ = INVALID_SOCKET;
        return false;
    }
    
    // ソケットを非ブロッキングに設定
    int flags = fcntl(can_socket_, F_GETFL, 0);
    fcntl(can_socket_, F_SETFL, flags | O_NONBLOCK);

    std::cout << "CANの初期化に成功しました." << std::endl;

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

void CanHandler::ReceiveCanMessages(int timeout_ms) {
    if (can_socket_ < 0) return;
    
    fd_set read_fds;
    struct timeval timeout;
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;
    
    FD_ZERO(&read_fds);
    FD_SET(can_socket_, &read_fds);
    
    int ret = select(can_socket_ + 1, &read_fds, nullptr, nullptr, &timeout);
    if (ret <= 0) return;  // タイムアウトまたはエラー
    
    struct can_frame frame;
    while (read(can_socket_, &frame, sizeof(frame)) > 0) {
        // ノードIDとコマンドIDを抽出
        const int node_id = (frame.can_id >> 5) & 0x3F;
        const int cmd_id = frame.can_id & 0x1F;
        
        // Get_Encoder_Estimates (0x09) メッセージを処理
        if (cmd_id == CMD_GET_ENCODER_ESTIMATES && frame.can_dlc == 8) {
            float pos_estimate, vel_estimate;
            std::memcpy(&pos_estimate, &frame.data[0], 4);
            std::memcpy(&vel_estimate, &frame.data[4], 4);
            
            // 現在時刻を取得
            auto now = std::chrono::steady_clock::now();
            double current_time = std::chrono::duration<double>(now.time_since_epoch()).count();
            
            // ノードIDに応じてストレージに保存 (node_id 1 = motor0, node_id 2 = motor1)
            if (node_id == 1) {
                motor0_encoder_storage_->Add(current_time, pos_estimate);
            } else if (node_id == 2) {
                motor1_encoder_storage_->Add(current_time, pos_estimate);
            }
        }
    }
}

void CanHandler::Update() {
    if (can_socket_ < 0) return;

    // キャリブレーション開始.
    SendAxisState(1, AXIS_STATE_FULL_CALIBRATION_SEQUENCE);
    SendAxisState(2, AXIS_STATE_FULL_CALIBRATION_SEQUENCE);

    // 30秒待機
    std::this_thread::sleep_for(std::chrono::seconds(30));

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

        // 目標値（Roadcell）と現在値（Potentiometer）を取得.
        const auto gravity = gravity_compensation_ * GRAVITY_ACCELERATION; 
        const auto error0 =gravity - roadcell_storage_->GetLatestValue();
        const auto d_error0 = -roadcell_storage_->GetLatestDifference();
        const auto error1 = potentiometer_storage_->GetLatestValue();
        const auto d_error1 = potentiometer_storage_->GetLatestDifference();
        
        // PD制御で誤差を計算.
        const double control_output_motor1 = pd_controller_motor1_.Compute(error0, d_error0);
        const double control_output_motor2 = pd_controller_motor2_.Compute(error1, d_error1);
        
        // 制御出力をストレージに保存.
        const double current_time = std::chrono::duration<double>(now.time_since_epoch()).count();
        motor0_control_storage_->Add(current_time, control_output_motor1);
        motor1_control_storage_->Add(current_time, control_output_motor2);
        
        // move_motors_がtrueの場合のみ速度指令を送信.
        if (move_motors_) {
            SendVelocity(1, static_cast<float>(control_output_motor1));
            SendVelocity(2, static_cast<float>(control_output_motor2));
        }
        
        // CANメッセージを受信してエンコーダ値を取得
        ReceiveCanMessages(1);  // 1msタイムアウト
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
