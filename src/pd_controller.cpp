#include "pd_controller.hpp"

namespace winch {

double PDController::Compute(const double error, const double d_error_dt) {
    return (kp_ * error) + (kd_ * d_error_dt);
}

}  // namespace winch
