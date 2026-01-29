#include <iostream>
#include <thread>
#include <chrono>
#include <cstring>
#include <cmath>

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

/* ---------- Gamepad settings ---------- */
constexpr const char* GAMEPAD_DEV = "/dev/input/event0"; // ←要確認
constexpr int AXIS_LY = 1;  // 左スティックY
constexpr int AXIS_RY = 4;  // 右スティックY

/* ---------- Control parameters ---------- */
constexpr float MAX_VEL = 5.0f;   // [turn/s]
constexpr float DEADZONE = 0.05f;
constexpr int   CONTROL_HZ = 100;

/* ---------- Globals ---------- */
int can_socket = -1;

/* ---------- CAN helpers ---------- */
void send_axis_state(int node_id, uint32_t state) {
    struct can_frame frame{};
    frame.can_id  = (node_id << 5) | CMD_SET_AXIS_REQUESTED_STATE;
    frame.can_dlc = 4;
    std::memcpy(frame.data, &state, 4);
    write(can_socket, &frame, sizeof(frame));
}

void send_velocity(int node_id, float vel_turn_s) {
    struct can_frame frame{};
    frame.can_id  = (node_id << 5) | CMD_SET_INPUT_VEL;
    frame.can_dlc = 4;
    std::memcpy(frame.data, &vel_turn_s, 4);
    write(can_socket, &frame, sizeof(frame));
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
    ioctl(can_socket, SIOCGIFINDEX, &ifr);

    struct sockaddr_can addr{};
    addr.can_family  = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    bind(can_socket, (struct sockaddr*)&addr, sizeof(addr));
    std::cout << "CAN initialized\n";

    /* ---------- Gamepad init ---------- */
    int pad_fd = open(GAMEPAD_DEV, O_RDONLY | O_NONBLOCK);
    if (pad_fd < 0) {
        perror("Gamepad open");
        return 1;
    }
    std::cout << "Gamepad opened\n";

    /* ---------- Calibration ---------- */
    std::cout << "Start calibration\n";
    send_axis_state(1, AXIS_STATE_FULL_CALIBRATION_SEQUENCE);
    send_axis_state(2, AXIS_STATE_FULL_CALIBRATION_SEQUENCE);

    std::this_thread::sleep_for(std::chrono::seconds(30));

    send_axis_state(1, AXIS_STATE_CLOSED_LOOP_CONTROL);
    send_axis_state(2, AXIS_STATE_CLOSED_LOOP_CONTROL);

    std::cout << "Closed loop control\n";

    /* ---------- Control loop ---------- */
    float vel1 = 0.0f;
    float vel2 = 0.0f;

    struct input_event ev{};

    while (true) {
        // --- Read controller ---
        while (read(pad_fd, &ev, sizeof(ev)) == sizeof(ev)) {
            if (ev.type == EV_ABS) {
                if (ev.code == AXIS_LY) {
                    vel1 = -normalize_axis(ev.value) * MAX_VEL;
                }
                else if (ev.code == AXIS_RY) {
                    vel2 = -normalize_axis(ev.value) * MAX_VEL;
                }
            }
        }

        // --- Send velocity ---
        send_velocity(1, vel1);
        send_velocity(2, vel2);

        std::this_thread::sleep_for(
            std::chrono::milliseconds(1000 / CONTROL_HZ)
        );
    }

    close(pad_fd);
    close(can_socket);
    return 0;
}
