#include <iostream>
#include <thread>
#include <chrono>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <cerrno>

#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>

#include <linux/can.h>
#include <linux/can/raw.h>
#include <linux/input.h>

/* ---------- ODrive CAN commands ---------- */
constexpr uint16_t CMD_SET_AXIS_REQUESTED_STATE = 0x007;
constexpr uint16_t CMD_SET_INPUT_VEL            = 0x00D;

/* ---------- ODrive states ---------- */
constexpr uint32_t AXIS_STATE_FULL_CALIBRATION_SEQUENCE = 3;
constexpr uint32_t AXIS_STATE_CLOSED_LOOP_CONTROL       = 8;

/* ---------- Gamepad (Logicool F310) ---------- */
constexpr const char* GAMEPAD_DEV =
    "/dev/input/by-id/usb-Logicool_Gamepad_F310_F9BC2D6C-event-joystick";

/* Axes */
constexpr int AXIS_LY = ABS_Y;   // 1
constexpr int AXIS_RY = ABS_RY;  // 4
constexpr int AXIS_LT = ABS_Z;   // 2
constexpr int AXIS_RT = ABS_RZ;  // 5

/* Buttons */
constexpr int BTN_LB = BTN_TL;   // 310
constexpr int BTN_RB = BTN_TR;   // 311

/* ---------- Control parameters ---------- */
constexpr float MAX_VEL = 5.0f;     // [turn/s]
constexpr float DEADZONE = 0.05f;
constexpr int   CONTROL_HZ = 100;

constexpr float LOW_GAIN  = 0.3f;   // 低速倍率
constexpr float HIGH_GAIN = 2.0f;   // 高速倍率
constexpr int   TRIGGER_ON_THRESH = 20;  // 0-255 でONとみなす値

/* ---------- Globals ---------- */
int can_socket = -1;

/* ---------- CAN helpers ---------- */
void send_axis_state(int node_id, uint32_t state) {
    struct can_frame frame{};
    frame.can_id  = (node_id << 5) | CMD_SET_AXIS_REQUESTED_STATE;
    frame.can_dlc = 4;
    std::memcpy(frame.data, &state, 4);

    if (write(can_socket, &frame, sizeof(frame)) != sizeof(frame)) {
        perror("CAN write axis state");
    }
}

void send_velocity(int node_id, float vel_turn_s) {
    struct can_frame frame{};
    frame.can_id  = (node_id << 5) | CMD_SET_INPUT_VEL;
    frame.can_dlc = 4;
    std::memcpy(frame.data, &vel_turn_s, 4);

    if (write(can_socket, &frame, sizeof(frame)) != sizeof(frame)) {
        perror("CAN write velocity");
    }
}

/* ---------- Utility ---------- */
float normalize_axis(int value) {
    float norm = value / 32767.0f;
    if (std::fabs(norm) < DEADZONE) return 0.0f;
    return norm;
}

int main() {
    /* ---------- CAN init ---------- */
    can_socket = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (can_socket < 0) {
        perror("CAN socket");
        return 1;
    }

    struct ifreq ifr{};
    std::strcpy(ifr.ifr_name, "can0");
    if (ioctl(can_socket, SIOCGIFINDEX, &ifr) < 0) {
        perror("SIOCGIFINDEX");
        return 1;
    }

    struct sockaddr_can addr{};
    addr.can_family  = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(can_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("CAN bind");
        return 1;
    }

    std::cout << "CAN initialized\n";

    /* ---------- Gamepad init ---------- */
    int pad_fd = open(GAMEPAD_DEV, O_RDONLY | O_NONBLOCK);
    if (pad_fd < 0) {
        perror("Gamepad open");
        return 1;
    }

    std::cout << "Gamepad opened: " << GAMEPAD_DEV << "\n";

    /* ---------- Calibration ---------- */
    std::cout << "Start calibration\n";
    send_axis_state(1, AXIS_STATE_FULL_CALIBRATION_SEQUENCE);
    send_axis_state(2, AXIS_STATE_FULL_CALIBRATION_SEQUENCE);

    std::this_thread::sleep_for(std::chrono::seconds(30));

    send_axis_state(1, AXIS_STATE_CLOSED_LOOP_CONTROL);
    send_axis_state(2, AXIS_STATE_CLOSED_LOOP_CONTROL);

    std::cout << "Closed loop control\n";

    /* ---------- Control state ---------- */
    float stick_norm1 = 0.0f;
    float stick_norm2 = 0.0f;

    bool low_mode_left  = false;
    bool low_mode_right = false;

    bool high_mode_left  = false;
    bool high_mode_right = false;

    struct input_event ev{};

    /* ---------- Control loop ---------- */
    while (true) {
        ssize_t n = read(pad_fd, &ev, sizeof(ev));

        if (n == 0) {
            std::cerr << "Gamepad disconnected (EOF)\n";
            break;
        }
        if (n < 0) {
            if (errno == ENODEV) {
                std::cerr << "Gamepad disconnected (ENODEV)\n";
                break;
            }
            // EAGAIN は無視
        }

        if (n == sizeof(ev)) {
            if (ev.type == EV_ABS) {
                if (ev.code == AXIS_LY) {
                    stick_norm1 = -normalize_axis(ev.value);
                }
                else if (ev.code == AXIS_RY) {
                    stick_norm2 = -normalize_axis(ev.value);
                }
                else if (ev.code == AXIS_LT) {
                    // LT: 左 高速モード
                    high_mode_left = (ev.value > TRIGGER_ON_THRESH);
                }
                else if (ev.code == AXIS_RT) {
                    // RT: 右 高速モード
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

        float gain_left  = 1.0f;
        float gain_right = 1.0f;

        if (low_mode_left)   gain_left  = LOW_GAIN;
        if (high_mode_left)  gain_left  = HIGH_GAIN;

        if (low_mode_right)  gain_right = LOW_GAIN;
        if (high_mode_right) gain_right = HIGH_GAIN;

        // LB/LT だけでは動かない（stick=0なら必ず0）
        float vel1 = stick_norm1 * MAX_VEL * gain_left;
        float vel2 = stick_norm2 * MAX_VEL * gain_right;

        vel1 = std::clamp(vel1, -MAX_VEL, MAX_VEL);
        vel2 = std::clamp(vel2, -MAX_VEL, MAX_VEL);

        send_velocity(1, vel1);
        send_velocity(2, vel2);

        std::this_thread::sleep_for(
            std::chrono::milliseconds(1000 / CONTROL_HZ)
        );
    }

    /* ---------- Fail-safe stop ---------- */
    std::cerr << "Stopping motors\n";
    send_velocity(1, 0.0f);
    send_velocity(2, 0.0f);

    close(pad_fd);
    close(can_socket);
    return 0;
}
