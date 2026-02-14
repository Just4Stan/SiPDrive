#include "position_control.h"

#include "math_compat.h"

namespace sipdrive::position_control {
namespace {

float Clamp(float v, float lo, float hi) {
  if (v < lo) { return lo; }
  if (v > hi) { return hi; }
  return v;
}

}  // namespace

void Reset(PositionState* state) {
  state->vel_integrator = 0.0f;
}

float Update(const PositionCommand& cmd,
             float position_rad,
             float velocity_rad_s,
             float dt_s,
             const PositionConfig& config,
             PositionState* state) {
  float vel_cmd = cmd.velocity_rad_s;

  // Position loop (skip if position is NaN).
  if (!isnan(cmd.position_rad)) {
    const float pos_error = cmd.position_rad - position_rad;
    vel_cmd += pos_error * config.pos_kp * cmd.kp_scale;
  }

  vel_cmd = Clamp(vel_cmd, -config.vel_limit_rad_s, config.vel_limit_rad_s);

  // Velocity loop.
  const float vel_error = vel_cmd - velocity_rad_s;
  const float iq_p = vel_error * config.vel_kp * cmd.kd_scale;

  state->vel_integrator += vel_error * config.vel_ki * dt_s;
  state->vel_integrator =
      Clamp(state->vel_integrator, -config.vel_ilimit, config.vel_ilimit);

  float iq_cmd = iq_p + state->vel_integrator;
  iq_cmd = Clamp(iq_cmd, -cmd.max_torque_a, cmd.max_torque_a);

  return iq_cmd;
}

}  // namespace sipdrive::position_control
