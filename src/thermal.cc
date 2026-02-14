#include "thermal.h"

#include "config.h"

namespace sipdrive::thermal {
namespace {

// 10k NTC B3950 lookup table: voltage (3.3V supply, 10k pullup to 3.3V) to
// temperature.  V = 3.3 * Rntc / (Rntc + 10k).
// Piecewise linear interpolation between these points.
struct VtEntry {
  float voltage;
  float temp_c;
};

constexpr VtEntry kNtcTable[] = {
    {2.81f, 0.0f},    // ~32.7k
    {2.48f, 25.0f},   // 10k
    {1.95f, 50.0f},   // ~4.16k
    {1.39f, 75.0f},   // ~1.73k
    {0.92f, 100.0f},  // ~0.84k
    {0.58f, 125.0f},  // ~0.44k
    {0.36f, 150.0f},  // ~0.24k
};
constexpr uint32_t kNtcTableSize =
    sizeof(kNtcTable) / sizeof(kNtcTable[0]);

float NtcVoltageToTemp(float voltage) {
  if (voltage >= kNtcTable[0].voltage) {
    return kNtcTable[0].temp_c;
  }
  if (voltage <= kNtcTable[kNtcTableSize - 1].voltage) {
    return kNtcTable[kNtcTableSize - 1].temp_c;
  }
  for (uint32_t i = 0; i < kNtcTableSize - 1U; ++i) {
    if (voltage <= kNtcTable[i].voltage &&
        voltage >= kNtcTable[i + 1].voltage) {
      const float frac =
          (kNtcTable[i].voltage - voltage) /
          (kNtcTable[i].voltage - kNtcTable[i + 1].voltage);
      return kNtcTable[i].temp_c +
             frac * (kNtcTable[i + 1].temp_c - kNtcTable[i].temp_c);
    }
  }
  return kNtcTable[kNtcTableSize - 1].temp_c;
}

float Clamp01(float v) {
  if (v < 0.0f) { return 0.0f; }
  if (v > 1.0f) { return 1.0f; }
  return v;
}

ThermalConfig g_config = {120.0f, 150.0f, 25.0f};
volatile float g_current_scale = 1.0f;
volatile bool g_fault = false;
float g_board_temp_c = 25.0f;
float g_stator_temp_c = 25.0f;

float ComputeScale(float temp_c, float fault_temp, float margin) {
  const float derate_start = fault_temp - margin;
  if (temp_c <= derate_start) {
    return 1.0f;
  }
  if (temp_c >= fault_temp) {
    return 0.0f;
  }
  return Clamp01(1.0f - ((temp_c - derate_start) / margin));
}

}  // namespace

void Init(const ThermalConfig& config) {
  g_config = config;
  g_current_scale = 1.0f;
  g_fault = false;
}

void Update(float board_ntc_v, float stator_ntc_v) {
  g_board_temp_c = NtcVoltageToTemp(board_ntc_v);
  g_stator_temp_c = NtcVoltageToTemp(stator_ntc_v);

  const float board_scale =
      ComputeScale(g_board_temp_c, g_config.board_fault_temp_c,
                   g_config.warning_margin_c);
  const float stator_scale =
      ComputeScale(g_stator_temp_c, g_config.stator_fault_temp_c,
                   g_config.warning_margin_c);

  float scale = board_scale;
  if (stator_scale < scale) {
    scale = stator_scale;
  }

  g_current_scale = scale;
  g_fault = (scale <= 0.0f);
}

float GetCurrentScale() {
  return g_current_scale;
}

bool IsFaulted() {
  return g_fault;
}

float GetBoardTempC() {
  return g_board_temp_c;
}

float GetStatorTempC() {
  return g_stator_temp_c;
}

}  // namespace sipdrive::thermal
