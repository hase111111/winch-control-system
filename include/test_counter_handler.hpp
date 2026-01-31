#ifndef TEST_COUNTER_HANDLER_HPP
#define TEST_COUNTER_HANDLER_HPP

#include <atomic>
#include <memory>

#include "time_series_storage.hpp"

#include "i_handler.hpp"

namespace winch {

//! @brief 100Hzでカウントアップするだけのテスト用ハンドラー.
class TestCounterHandler final : public IHandler {
public:
    TestCounterHandler(const std::atomic_bool& stop_flag,
                       const std::shared_ptr<TimeSeriesStorage>& storage);

    bool Initialize() override;
    void Update() override;
    void Finalize() override;

private:
    const std::atomic_bool& stop_flag_;
    const std::shared_ptr<TimeSeriesStorage> storage_;
    uint64_t count_;
};

}  // namespace winch

#endif // TEST_COUNTER_HANDLER_HPP
