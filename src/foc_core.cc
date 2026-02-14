#include "foc_core.h"

#include "config.h"
#include "stm32g431xx.h"

namespace sipdrive::foc {
namespace {

constexpr float kInvSqrt3 = 0.57735026919f;
constexpr float kSqrt3Over2 = 0.86602540378f;
constexpr float kPi = 3.14159265359f;

float Clamp(float value, float min_value, float max_value) {
  if (value < min_value) {
    return min_value;
  }
  if (value > max_value) {
    return max_value;
  }
  return value;
}

// CORDIC CSR configuration:
// FUNC=0 (cosine), PRECISION=5 (5 iterations), SCALE=0,
// NRES=1 (two results: cos then sin), NARGS=0 (one argument),
// ARGSIZE=0 (32-bit Q1.31), RESSIZE=0 (32-bit Q1.31).
constexpr uint32_t kCordicCsr =
    (5U << 4U) |   // PRECISION = 5 iterations
    (1U << 19U);   // NRES = 1 (read two results)

constexpr float kQ31ToFloat = 1.0f / 2147483648.0f;
constexpr float kRadToQ31 = 2147483648.0f / kPi;

struct SinCos {
  float sin;
  float cos;
};

SinCos CordicSinCos(float angle_rad) {
  const int32_t q31 = static_cast<int32_t>(angle_rad * kRadToQ31);
  CORDIC->WDATA = static_cast<uint32_t>(q31);
  SinCos result;
  result.cos = static_cast<float>(static_cast<int32_t>(CORDIC->RDATA)) * kQ31ToFloat;
  result.sin = static_cast<float>(static_cast<int32_t>(CORDIC->RDATA)) * kQ31ToFloat;
  return result;
}

}  // namespace

void FocCore::Reset() {
  // Enable CORDIC clock and configure for cosine, 5-cycle precision, Q31.
  RCC->AHB1ENR |= RCC_AHB1ENR_CORDICEN;
  (void)RCC->AHB1ENR;
  CORDIC->CSR = kCordicCsr;

  id_integrator_ = 0.0f;
  iq_integrator_ = 0.0f;
}

FocOutput FocCore::Step(const FocInputs& inputs, const FocCommand& command) {
  const SinCos sc = CordicSinCos(inputs.electrical_angle_rad);
  const float sin_t = sc.sin;
  const float cos_t = sc.cos;

  const float i_alpha = inputs.ia_a;
  const float i_beta = (inputs.ia_a + 2.0f * inputs.ib_a) * kInvSqrt3;

  const float id = (cos_t * i_alpha) + (sin_t * i_beta);
  const float iq = (-sin_t * i_alpha) + (cos_t * i_beta);

  const float id_error = command.id_a - id;
  const float iq_error = command.iq_a - iq;

  id_integrator_ += config::kIdKi * id_error * inputs.dt_s;
  iq_integrator_ += config::kIqKi * iq_error * inputs.dt_s;

  const float vmax = config::kBusVoltageV * 0.5f;
  id_integrator_ = Clamp(id_integrator_, -vmax, vmax);
  iq_integrator_ = Clamp(iq_integrator_, -vmax, vmax);

  float vd = (config::kIdKp * id_error) + id_integrator_;
  float vq = (config::kIqKp * iq_error) + iq_integrator_;

  vd = Clamp(vd, -vmax, vmax);
  vq = Clamp(vq, -vmax, vmax);

  const float v_alpha = (cos_t * vd) - (sin_t * vq);
  const float v_beta = (sin_t * vd) + (cos_t * vq);

  float va = v_alpha;
  float vb = (-0.5f * v_alpha) + (kSqrt3Over2 * v_beta);
  float vc = (-0.5f * v_alpha) - (kSqrt3Over2 * v_beta);

  float v_min = va;
  float v_max = va;
  if (vb < v_min) {
    v_min = vb;
  }
  if (vc < v_min) {
    v_min = vc;
  }
  if (vb > v_max) {
    v_max = vb;
  }
  if (vc > v_max) {
    v_max = vc;
  }

  // Center common-mode voltage for SVPWM-like duty generation.
  const float v_common = 0.5f * (v_max + v_min);
  va -= v_common;
  vb -= v_common;
  vc -= v_common;

  const float duty_scale = 1.0f / config::kBusVoltageV;
  FocOutput output;
  output.duty.phase_a = Clamp(0.5f + (va * duty_scale), config::kDutyMin, config::kDutyMax);
  output.duty.phase_b = Clamp(0.5f + (vb * duty_scale), config::kDutyMin, config::kDutyMax);
  output.duty.phase_c = Clamp(0.5f + (vc * duty_scale), config::kDutyMin, config::kDutyMax);
  output.vd_v = vd;
  output.vq_v = vq;
  output.id_meas_a = id;
  output.iq_meas_a = iq;
  return output;
}

}  // namespace sipdrive::foc
