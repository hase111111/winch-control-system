#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>

int main() {
    const char* device = "/dev/input/event0"; // ←適宜変更
    int fd = open(device, O_RDONLY);

    if (fd < 0) {
        perror("Failed to open device");
        return 1;
    }

    struct input_event ev;

    std::cout << "Listening to controller input..." << std::endl;

    while (true) {
        ssize_t n = read(fd, &ev, sizeof(ev));
        if (n != sizeof(ev)) {
            perror("Read error");
            break;
        }

        // ボタン入力
        if (ev.type == EV_KEY) {
            std::cout << "[KEY] code=" << ev.code
                      << " value=" << ev.value << std::endl;
        }

        // アナログスティック
        if (ev.type == EV_ABS) {
            std::cout << "[AXIS] code=" << ev.code
                      << " value=" << ev.value << std::endl;
        }
    }

    close(fd);
    return 0;
}
