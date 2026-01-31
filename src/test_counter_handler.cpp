#include "test_counter_handler.hpp"

#include <chrono>
#include <thread>

namespace winch {

TestCounterHandler::TestCounterHandler(const std::atomic_bool& stop_flag,
                                       const std::shared_ptr<TimeSeriesStorage>& storage)
    : stop_flag_(stop_flag), storage_(storage), count_(0) {}

bool TestCounterHandler::Initialize() {
    count_ = 0;
    return true;
}

void TestCounterHandler::Update() {
    constexpr int kHz100_ms = 10;  // 100 Hz = 10 ms
    const auto start_time = std::chrono::steady_clock::now();
    auto next_tick = std::chrono::steady_clock::now();

    while (!stop_flag_.load()) {
        const auto now = std::chrono::steady_clock::now();
        if (now < next_tick) {
            std::this_thread::sleep_for(next_tick - now);
        }
        next_tick += std::chrono::milliseconds(kHz100_ms);

        ++count_;
        if (storage_) {
            const std::chrono::duration<double> elapsed = now - start_time;
            const double time_sec = elapsed.count();
            storage_->Add(time_sec, static_cast<double>(count_));
        }
    }
}

void TestCounterHandler::Finalize() {
    // 何もしない
}

}  // namespace winch
