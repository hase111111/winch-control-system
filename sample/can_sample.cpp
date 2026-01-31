#include <iostream>
#include <thread>
#include <chrono>
#include <cstring>

#include <sys/socket.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/can.h>
#include <linux/can/raw.h>

constexpr uint16_t CMD_SET_AXIS_REQUESTED_STATE = 0x007;
constexpr uint16_t CMD_SET_INPUT_POS            = 0x00C;

constexpr uint32_t AXIS_STATE_FULL_CALIBRATION_SEQUENCE = 3;
constexpr uint32_t AXIS_STATE_CLOSED_LOOP_CONTROL       = 8;

int can_socket = -1;

void send_axis_state(int node_id, uint32_t state) {
    struct can_frame frame{};
    frame.can_id  = (node_id << 5) | CMD_SET_AXIS_REQUESTED_STATE;
    frame.can_dlc = 4;
    std::memcpy(frame.data, &state, 4);
    write(can_socket, &frame, sizeof(frame));
}

void send_position(int node_id, float pos_turn) {
    struct can_frame frame{};
    frame.can_id  = (node_id << 5) | CMD_SET_INPUT_POS;
    frame.can_dlc = 4;
    std::memcpy(frame.data, &pos_turn, 4);
    write(can_socket, &frame, sizeof(frame));
}

int main() {
    /* ---------- CAN初期化 ---------- */
    can_socket = socket(PF_CAN, SOCK_RAW, CAN_RAW);

    struct ifreq ifr{};
    std::strcpy(ifr.ifr_name, "can0");
    ioctl(can_socket, SIOCGIFINDEX, &ifr);

    struct sockaddr_can addr{};
    addr.can_family  = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    bind(can_socket, (struct sockaddr*)&addr, sizeof(addr));

    std::cout << "CAN initialized\n";

    /* ---------- キャリブレーション ---------- */
    std::cout << "Start calibration\n";
    send_axis_state(1, AXIS_STATE_FULL_CALIBRATION_SEQUENCE);
    send_axis_state(2, AXIS_STATE_FULL_CALIBRATION_SEQUENCE);

    std::this_thread::sleep_for(std::chrono::seconds(30));

    send_axis_state(1, AXIS_STATE_CLOSED_LOOP_CONTROL);
    send_axis_state(2, AXIS_STATE_CLOSED_LOOP_CONTROL);

    std::cout << "Calibration done, enter closed loop\n";

    std::this_thread::sleep_for(std::chrono::seconds(1));

    /* ---------- ゆっくり1回転 ---------- */
    constexpr float target_turn = 1.0f;   // 1回転
    constexpr float duration    = 10.0f;  // 10秒かける
    constexpr int   hz          = 100;

    int steps = duration * hz;

    for (int i = 0; i <= steps; i++) {
        float pos = target_turn * i / steps;

        send_position(1, pos);
        send_position(2, pos);

        std::this_thread::sleep_for(std::chrono::milliseconds(1000 / hz));
    }

    std::cout << "Rotation finished\n";

    close(can_socket);
    return 0;
}
