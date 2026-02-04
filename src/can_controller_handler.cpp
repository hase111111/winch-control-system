#include "can_controller_handler.hpp"

#include <iostream>
#include <cstring>
#include <thread>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <cerrno>

#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <linux/input.h>

namespace winch {

namespace {

constexpr int INVALID_SOCKET = -1;
constexpr int INVALID_FD = -1;

// ODrive CAN コマンド
constexpr uint16_t CMD_SET_AXIS_REQUESTED_STATE = 0x007;
constexpr uint16_t CMD_GET_ENCODER_ESTIMATES = 0x009;
constexpr uint16_t CMD_SET_INPUT_VEL = 0x00D;

// ODrive 軸状態
constexpr uint32_t AXIS_STATE_IDLE = 1;
constexpr uint32_t AXIS_STATE_FULL_CALIBRATION_SEQUENCE = 3;
constexpr uint32_t AXIS_STATE_CLOSED_LOOP_CONTROL = 8;

// ゲームパッド軸・ボタン定義（Logicool F310）
constexpr int AXIS_LY = ABS_Y;   // 左スティックY
constexpr int AXIS_RY = ABS_RY;  // 右スティックY
constexpr int AXIS_LT = ABS_Z;   // 左トリガー
constexpr int AXIS_RT = ABS_RZ;  // 右トリガー
constexpr int BTN_LB = BTN_TL;   // 左バンパー
constexpr int BTN_RB = BTN_TR;   // 右バンパー

constexpr int TRIGGER_ON_THRESH = 20;  // トリガーがONとみなす閾値

}  // namespace

CanControllerHandler::CanControllerHandler(
    const ConfigLoader& config,
    const std::atomic_bool& stop_flag,
    std::shared_ptr<TimeSeriesStorage> motor0_control_storage,
    std::shared_ptr<TimeSeriesStorage> motor1_control_storage,
    std::shared_ptr<TimeSeriesStorage> motor0_encoder_storage,
    std::shared_ptr<TimeSeriesStorage> motor1_encoder_storage)
    : interface_name_(config.GetVal<std::string>("CAN", "interface", "can0")),
      gamepad_device_(config.GetVal<std::string>("Controller", "device", 
          "/dev/input/by-id/usb-Logicool_Gamepad_F310_F9BC2D6C-event-joystick")),
      stop_flag_(stop_flag),
      can_socket_(INVALID_SOCKET),
      gamepad_fd_(INVALID_FD),
      motor0_control_storage_(motor0_control_storage),
      motor1_control_storage_(motor1_control_storage),
      motor0_encoder_storage_(motor0_encoder_storage),
      motor1_encoder_storage_(motor1_encoder_storage),
      max_velocity_(config.GetVal<float>("Controller", "max_velocity", 5.0f)),
      deadzone_(config.GetVal<float>("Controller", "deadzone", 0.05f)),
      low_gain_(config.GetVal<float>("Controller", "low_gain", 0.3f)),
      high_gain_(config.GetVal<float>("Controller", "high_gain", 2.0f)),
      move_motors_(config.GetVal<bool>("Flags", "move_morotors", true)) {
    
    std::cout << "CAN interface: " << interface_name_ << std::endl;
    std::cout << "Gamepad device: " << gamepad_device_ << std::endl;
    std::cout << "Max velocity: " << max_velocity_ << " turn/s" << std::endl;
    std::cout << "モータ移動フラグ: " << (move_motors_ ? "有効" : "無効") << std::endl;
}

bool CanControllerHandler::Initialize() {
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

    // ゲームパッドを開く
    gamepad_fd_ = open(gamepad_device_.c_str(), O_RDONLY | O_NONBLOCK);
    if (gamepad_fd_ < 0) {
        std::cerr << "ゲームパッドのオープンに失敗しました: " << gamepad_device_ << std::endl;
        close(can_socket_);
        can_socket_ = INVALID_SOCKET;
        return false;
    }

    std::cout << "ゲームパッドの初期化に成功しました." << std::endl;

    return true;
}

void CanControllerHandler::SendAxisState(const int node_id, const uint32_t state) {
    if (can_socket_ < 0) return;

    struct can_frame frame {};
    frame.can_id = (node_id << 5) | CMD_SET_AXIS_REQUESTED_STATE;
    frame.can_dlc = 4;
    std::memcpy(frame.data, &state, 4);
    write(can_socket_, &frame, sizeof(frame));
}

void CanControllerHandler::SendVelocity(const int node_id, const float vel_turn_s) {
    if (can_socket_ < 0) return;

    struct can_frame frame {};
    frame.can_id = (node_id << 5) | CMD_SET_INPUT_VEL;
    frame.can_dlc = 4;
    std::memcpy(frame.data, &vel_turn_s, 4);
    write(can_socket_, &frame, sizeof(frame));
}

void CanControllerHandler::ReceiveCanMessages(int timeout_ms) {
    if (can_socket_ < 0) return;
    
    fd_set read_fds;
    struct timeval timeout;
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;
    
    FD_ZERO(&read_fds);
    FD_SET(can_socket_, &read_fds);
    
    int ret = select(can_socket_ + 1, &read_fds, nullptr, nullptr, &timeout);
    if (ret <= 0) return;
    
    struct can_frame frame;
    while (read(can_socket_, &frame, sizeof(frame)) > 0) {
        const int node_id = (frame.can_id >> 5) & 0x3F;
        const int cmd_id = frame.can_id & 0x1F;
        
        if (cmd_id == CMD_GET_ENCODER_ESTIMATES && frame.can_dlc == 8) {
            float pos_estimate, vel_estimate;
            std::memcpy(&pos_estimate, &frame.data[0], 4);
            std::memcpy(&vel_estimate, &frame.data[4], 4);
            
            auto now = std::chrono::steady_clock::now();
            double current_time = std::chrono::duration<double>(now.time_since_epoch()).count();
            
            if (node_id == 1) {
                motor0_encoder_storage_->Add(current_time, pos_estimate);
            } else if (node_id == 2) {
                motor1_encoder_storage_->Add(current_time, pos_estimate);
            }
        }
    }
}

float CanControllerHandler::NormalizeAxis(int value) {
    float norm = value / 32767.0f;
    if (std::fabs(norm) < deadzone_) return 0.0f;
    return norm;
}

void CanControllerHandler::Update() {
    if (can_socket_ < 0 || gamepad_fd_ < 0) return;

    // キャリブレーション開始
    std::cout << "キャリブレーション開始..." << std::endl;
    SendAxisState(1, AXIS_STATE_FULL_CALIBRATION_SEQUENCE);
    std::this_thread::sleep_for(std::chrono::seconds(10));
    SendAxisState(2, AXIS_STATE_FULL_CALIBRATION_SEQUENCE);

    // 30秒待機
    std::this_thread::sleep_for(std::chrono::seconds(15));

    if (stop_flag_.load()) return;

    // クローズドループ制御に移行
    std::cout << "クローズドループ制御開始" << std::endl;
    SendAxisState(1, AXIS_STATE_CLOSED_LOOP_CONTROL);
    SendAxisState(2, AXIS_STATE_CLOSED_LOOP_CONTROL);

    std::this_thread::sleep_for(std::chrono::seconds(1));

    // コントローラー入力状態
    float stick_norm1 = 0.0f;
    float stick_norm2 = 0.0f;
    bool low_mode_left = false;
    bool low_mode_right = false;
    bool high_mode_left = false;
    bool high_mode_right = false;

    // 制御ループ (100Hz)
    constexpr int hz = 100;
    auto next_send_time = std::chrono::steady_clock::now();

    while (!stop_flag_.load()) {
        auto now = std::chrono::steady_clock::now();
        if (now < next_send_time) {
            std::this_thread::sleep_for(next_send_time - now);
        }
        next_send_time += std::chrono::milliseconds(1000 / hz);

        // ゲームパッド入力を読み取る
        struct input_event ev {};
        ssize_t n = read(gamepad_fd_, &ev, sizeof(ev));

        if (n == 0) {
            std::cerr << "ゲームパッド切断 (EOF)" << std::endl;
            break;
        }
        if (n < 0) {
            if (errno == ENODEV) {
                std::cerr << "ゲームパッド切断 (ENODEV)" << std::endl;
                break;
            }
            // EAGAIN は無視
        }

        if (n == sizeof(ev)) {
            if (ev.type == EV_ABS) {
                if (ev.code == AXIS_LY) {
                    stick_norm1 = -NormalizeAxis(ev.value);
                }
                else if (ev.code == AXIS_RY) {
                    stick_norm2 = -NormalizeAxis(ev.value);
                }
                else if (ev.code == AXIS_LT) {
                    high_mode_left = (ev.value > TRIGGER_ON_THRESH);
                }
                else if (ev.code == AXIS_RT) {
                    high_mode_right = (ev.value > TRIGGER_ON_THRESH);
                }
            }
            else if (ev.type == EV_KEY) {
                if (ev.code == BTN_LB) {
                    low_mode_left = (ev.value != 0);
                }
                else if (ev.code == BTN_RB) {
                    low_mode_right = (ev.value != 0);
                }
            }
        }

        // ゲイン計算
        float gain_left = 1.0f;
        float gain_right = 1.0f;

        if (low_mode_left)   gain_left = low_gain_;
        if (high_mode_left)  gain_left = high_gain_;
        if (low_mode_right)  gain_right = low_gain_;
        if (high_mode_right) gain_right = high_gain_;

        // 速度計算
        float vel1 = stick_norm1 * max_velocity_ * gain_left;
        float vel2 = stick_norm2 * max_velocity_ * gain_right;

        vel1 = std::clamp(vel1, -max_velocity_ * 3.0f, max_velocity_ * 3.0f);
        vel2 = std::clamp(vel2, -max_velocity_ * 3.0f, max_velocity_ * 3.0f);

        // 制御出力をストレージに保存
        double current_time = std::chrono::duration<double>(now.time_since_epoch()).count();
        motor0_control_storage_->Add(current_time, vel1);
        motor1_control_storage_->Add(current_time, vel2);

        // move_motors_がtrueの場合のみ速度指令を送信
        if (move_motors_) {
            SendVelocity(1, vel1);
            SendVelocity(2, vel2);
        }

        // CANメッセージを受信してエンコーダ値を取得
        ReceiveCanMessages(1);
    }
}

void CanControllerHandler::Finalize() {
    if (can_socket_ >= 0) {
        // モータを停止させる
        SendVelocity(1, 0.0f);
        SendVelocity(2, 0.0f);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        SendAxisState(1, AXIS_STATE_IDLE);
        SendAxisState(2, AXIS_STATE_IDLE);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        close(can_socket_);
        can_socket_ = INVALID_SOCKET;
    }
    
    if (gamepad_fd_ >= 0) {
        close(gamepad_fd_);
        gamepad_fd_ = INVALID_FD;
    }
}

}  // namespace winch
