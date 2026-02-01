#include "pd_controller.hpp"

namespace winch {

double PDController::Compute(double error, double d_error_dt) {
    return (kp_ * error) + (kd_ * d_error_dt);
}

}  // namespace winch
