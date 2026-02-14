#pragma once

#include <stdint.h>

namespace sipdrive::thermal {

struct ThermalConfig {
  float board_fault_temp_c;
  float stator_fault_temp_c;
  float warning_margin_c;
};

void Init(const ThermalConfig& config);

// Call from main loop with latest NTC voltages.
void Update(float board_ntc_v, float stator_ntc_v);

// Current derating factor [0.0, 1.0].  Safe to call from ISR.
float GetCurrentScale();

// True if any thermal fault is active.
bool IsFaulted();

// Temperatures from last Update() call.
float GetBoardTempC();
float GetStatorTempC();

}  // namespace sipdrive::thermal
