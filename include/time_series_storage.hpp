#ifndef TIME_SERIES_STORAGE_HPP
#define TIME_SERIES_STORAGE_HPP

#include <mutex>
#include <utility>
#include <vector>

namespace winch {

class TimeSeriesStorage final {
public:
    void Add(double time_sec, double value);
    std::vector<std::pair<double, double>> GetSnapshot() const;

private:
    mutable std::mutex data_mutex_;
    std::vector<std::pair<double, double>> values_;
};

}  // namespace winch

#endif // TIME_SERIES_STORAGE_HPP
