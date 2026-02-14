#pragma once

#include "hal_tim1_pwm.h"

namespace sipdrive::foc {

struct FocInputs {
  float ia_a;
  float ib_a;
  float electrical_angle_rad;
  float dt_s;
};

struct FocCommand {
  float id_a;
  float iq_a;
};

struct FocOutput {
  hal::tim1_pwm::DutyCycles duty;
  float vd_v;
  float vq_v;
  float id_meas_a;
  float iq_meas_a;
};

class FocCore {
 public:
  void Reset();
  FocOutput Step(const FocInputs& inputs, const FocCommand& command);

 private:
  float id_integrator_ = 0.0f;
  float iq_integrator_ = 0.0f;
};

}  // namespace sipdrive::foc
