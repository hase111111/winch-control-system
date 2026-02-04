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

double TimeSeriesStorage::GetLatestValue() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    if (values_.empty()) {
        return 0.0;
    }
    return values_.back().second;
}

double TimeSeriesStorage::GetLatestTime() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    if (values_.empty()) {
        return 0.0;
    }
    return values_.back().first;
}

double TimeSeriesStorage::GetLatestDifference() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    if (values_.size() < 2) {
        return 0.0;
    }
    const auto& [t_n, v_n] = values_.back();
    const auto& [t_prev, v_prev] = values_[values_.size() - 2];
    const double delta_v = v_n - v_prev;
    const double delta_t = t_n - t_prev;
    return (delta_t != 0.0) ? (delta_v / delta_t) : 0.0;
}

double TimeSeriesStorage::GetAverageValue() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    if (values_.empty()) {
        return 0.0;
    }
    double sum = 0.0;
    for (const auto& [time, value] : values_) {
        sum += value;
    }
    return sum / static_cast<double>(values_.size());
}

}  // namespace winch
