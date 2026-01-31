#include "can_handler.hpp"

namespace winch {

CanHandler::CanHandler(std::atomic_bool& stop_flag)
    : stop_flag_(stop_flag) {}

void CanHandler::Update() {
    while (true) {
        if (stop_flag_) {
            // 終了フラグが立ったらループを抜ける．
            break;
        }
    }
}

}  // namespace winch
