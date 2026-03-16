#pragma once

#include <stdint.h>

#include "persistent_config.h"

namespace sipdrive::calibration {

enum class State {
  kIdle,
  kForward,
  kReverse,
  kCompute,
  kDone,
  kFailed,
};

struct CalibrationResult {
  float electrical_offset_rad;
  uint16_t compensation_table[persistent_config::kCalibTableSize];
  bool valid;
};

// Begin calibration.  Motor must be free to rotate.
void Start();

// Call from main loop at ~1 kHz.  Drives the motor open-loop during calibration.
// Returns phase duty cycles to apply (only valid when state is kForward/kReverse).
// duty_a/b/c are outputs in range [0,1].
State Update(float* duty_a, float* duty_b, float* duty_c);

// Get the result after state reaches kDone.
const CalibrationResult& GetResult();

// Returns true when calibration is actively driving the motor (kForward/kReverse).
// Safe to call from ISR context.
bool IsActive();

}  // namespace sipdrive::calibration
