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

    //! @return 最新の値を取得する.
    double GetLatestValue() const;

    //! @return 最新の時刻を取得する.
    double GetLatestTime() const;

    //! @return 最新の値の差分を取得する.
    //! v_n - v_(n-1) / t_n - t_(n-1) を返す.
    double GetLatestDifference() const;

    //! @brief 平均値を取得する.
    //! 全データの平均値を返す.
    double GetAverageValue() const;

private:
    mutable std::mutex data_mutex_;
    std::vector<std::pair<double, double>> values_;
};

}  // namespace winch

#endif // TIME_SERIES_STORAGE_HPP
