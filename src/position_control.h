#pragma once

#include "math_compat.h"

namespace sipdrive::position_control {

struct PositionCommand {
  float position_rad;       // NaN = disable position loop (velocity-only or current mode).
  float velocity_rad_s;     // Feedforward velocity (or target when position=NaN).
  float max_torque_a;       // Current limit for this command.
  float kp_scale;           // 0.0-1.0 multiplier on position Kp.
  float kd_scale;           // 0.0-1.0 multiplier on velocity Kp.
};

struct PositionConfig {
  float pos_kp;             // Position → velocity gain (rad/s per rad error).
  float vel_kp;             // Velocity → current gain (A per rad/s error).
  float vel_ki;             // Velocity integral gain.
  float vel_ilimit;         // Velocity integrator limit (A).
  float vel_limit_rad_s;    // Max velocity from position loop.
};

struct PositionState {
  float vel_integrator;
};

void Reset(PositionState* state);

// Run at control loop rate.  Returns Iq current command in amps.
float Update(const PositionCommand& cmd,
             float position_rad,
             float velocity_rad_s,
             float dt_s,
             const PositionConfig& config,
             PositionState* state);

}  // namespace sipdrive::position_control
