#ifndef PD_CONTROLLER_HPP
#define PD_CONTROLLER_HPP

namespace winch {

class PDController final {
public:
    PDController() = default;

    //! @brief ゲインを設定.
    inline void SetGains(const double kp, const double kd) {
        kp_ = kp;
        kd_ = kd;
    }

    //! @brief P ゲインを設定.
    inline void SetKp(const double kp) { kp_ = kp; }

    //! @brief D ゲインを設定.
    inline void SetKd(const double kd) { kd_ = kd; }

    //! @brief PD制御の計算.
    //! Kp * error + Kd * (d(error)/dt)
    //! @return 制御出力.
    double Compute(double error, double d_error_dt);

private:
    double kp_{};  //!< 比例ゲイン.
    double kd_{};  //!< 微分ゲイン.
};

}  // namespace winch

#endif  // PD_CONTROLLER_HPP
