#include "time_series_storage.hpp"

namespace winch {

void TimeSeriesStorage::Add(double time_sec, double value) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    values_.emplace_back(time_sec, value);
}

std::vector<std::pair<double, double>> TimeSeriesStorage::GetSnapshot() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    return values_;
}

}  // namespace winch
