#include "calibration.h"

#include "config.h"
#include "hal_qei.h"
#include "stm32g431xx.h"
#include "math_compat.h"

namespace sipdrive::calibration {
namespace {

constexpr float kPi = 3.14159265359f;
constexpr float kTwoPi = 6.28318530718f;

// Calibration voltage applied to d-axis (volts).
constexpr float kCalibVoltage = 2.0f;

// Number of steps per full electrical revolution.
constexpr uint32_t kStepsPerElecRev = 128U;

// Total steps = steps_per_elec_rev * pole_pairs.
// Settling time per step in Update() calls (~1 kHz assumed).
constexpr uint32_t kSettleTicks = 5U;

constexpr float kQ15Scale = 32768.0f;

State g_state = State::kIdle;
uint32_t g_step = 0U;
uint32_t g_total_steps = 0U;
uint32_t g_settle_count = 0U;
CalibrationResult g_result = {};

// Raw encoder readings at each step.
// Max total steps = 128 * pole_pairs.  For pole_pairs <= 14 this is <= 1792.
// We'll use a fixed buffer sized for reasonable motors.
constexpr uint32_t kMaxTotalSteps = 2048U;
int32_t g_forward_counts[kMaxTotalSteps];
int32_t g_reverse_counts[kMaxTotalSteps]
    __attribute__((section(".ccm_bss")));

float StepToElecAngle(uint32_t step) {
  return static_cast<float>(step) * kTwoPi /
         static_cast<float>(kStepsPerElecRev);
}

// Convert electrical angle to 3-phase duty cycles using inverse Park/Clarke
// with d-axis voltage only.
void ElecAngleToDuty(float theta, float vd, float* da, float* db, float* dc) {
  const float sin_t = sinf(theta);
  const float cos_t = cosf(theta);

  const float v_alpha = cos_t * vd;
  const float v_beta = sin_t * vd;

  const float inv_vbus = 1.0f / config::kBusVoltageV;
  *da = 0.5f + v_alpha * inv_vbus;
  *db = 0.5f + (-0.5f * v_alpha + 0.86602540378f * v_beta) * inv_vbus;
  *dc = 0.5f + (-0.5f * v_alpha - 0.86602540378f * v_beta) * inv_vbus;
}

void ComputeResult() {
  const uint32_t n = g_total_steps;
  const uint32_t pp = config::kMotorPolePairs;
  const uint32_t cpr = config::kEncoderCountsPerRev;

  // Average forward and reverse to cancel cogging/friction offset.
  // Compute expected_angle - measured_angle for each step.

  // Find the encoder count at electrical angle 0 (average of fwd[0] and rev[n-1]).
  const float offset_counts =
      0.5f * static_cast<float>(g_forward_counts[0] + g_reverse_counts[0]);
  g_result.electrical_offset_rad =
      offset_counts * (kTwoPi / static_cast<float>(cpr)) *
      static_cast<float>(pp);

  // Build 128-entry compensation table indexed by mechanical angle.
  // Each entry maps a mechanical angle bin to the correction in Q1.15 radians.
  for (uint32_t i = 0; i < persistent_config::kCalibTableSize; ++i) {
    // Mechanical angle for this bin center.
    const float mech_angle =
        (static_cast<float>(i) + 0.5f) * kTwoPi /
        static_cast<float>(persistent_config::kCalibTableSize);

    // Find the closest calibration step to this mechanical angle.
    // Mechanical angle per step = (2pi * pole_pairs) / total_steps * (1/pole_pairs)
    //                            = 2pi / total_steps
    const float step_per_rad =
        static_cast<float>(n) / kTwoPi;
    uint32_t best_step = static_cast<uint32_t>(mech_angle * step_per_rad);
    if (best_step >= n) {
      best_step = n - 1U;
    }

    // Expected electrical angle at this step.
    const float expected_elec = StepToElecAngle(best_step);

    // Measured electrical angle from encoder.
    const float avg_count = 0.5f * static_cast<float>(
        g_forward_counts[best_step] + g_reverse_counts[best_step]);
    const float measured_mech =
        avg_count * (kTwoPi / static_cast<float>(cpr));
    const float measured_elec =
        measured_mech * static_cast<float>(pp);

    // Correction = expected - measured (in electrical radians).
    float correction = expected_elec - measured_elec;
    // Wrap to [-pi, pi].
    correction = fmodf(correction, kTwoPi);
    if (correction > kPi) { correction -= kTwoPi; }
    if (correction < -kPi) { correction += kTwoPi; }

    // Store as signed Q1.15.
    float scaled = correction * (kQ15Scale / kPi);
    if (scaled > 32767.0f) { scaled = 32767.0f; }
    if (scaled < -32768.0f) { scaled = -32768.0f; }
    g_result.compensation_table[i] =
        static_cast<uint16_t>(static_cast<int16_t>(scaled));
  }

  g_result.valid = true;
}

}  // namespace

void Start() {
  g_step = 0U;
  g_total_steps = kStepsPerElecRev * config::kMotorPolePairs;
  if (g_total_steps > kMaxTotalSteps) {
    g_total_steps = kMaxTotalSteps;
  }
  g_settle_count = 0U;
  g_result.valid = false;
  g_state = State::kForward;
}

State Update(float* duty_a, float* duty_b, float* duty_c) {
  if (g_state == State::kIdle || g_state == State::kDone ||
      g_state == State::kFailed) {
    return g_state;
  }

  if (g_state == State::kCompute) {
    ComputeResult();
    g_state = State::kDone;
    return g_state;
  }

  const bool is_forward = (g_state == State::kForward);
  const float theta = is_forward
      ? StepToElecAngle(g_step)
      : StepToElecAngle(g_total_steps - 1U - g_step);

  ElecAngleToDuty(theta, kCalibVoltage, duty_a, duty_b, duty_c);

  ++g_settle_count;
  if (g_settle_count < kSettleTicks) {
    return g_state;
  }
  g_settle_count = 0U;

  // Record encoder count.
  const int32_t counts = hal::qei::GetPositionCounts();
  if (is_forward) {
    g_forward_counts[g_step] = counts;
  } else {
    g_reverse_counts[g_total_steps - 1U - g_step] = counts;
  }

  ++g_step;
  if (g_step >= g_total_steps) {
    if (is_forward) {
      g_step = 0U;
      g_state = State::kReverse;
    } else {
      g_state = State::kCompute;
    }
  }

  return g_state;
}

const CalibrationResult& GetResult() {
  return g_result;
}

bool IsActive() {
  return (g_state == State::kForward || g_state == State::kReverse);
}

}  // namespace sipdrive::calibration
