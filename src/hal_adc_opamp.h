#pragma once

#include <stdint.h>

namespace sipdrive::hal::adc_opamp {

struct CurrentSample {
  uint32_t timestamp_ticks;
  uint16_t raw_ia;
  uint16_t raw_ib;
  float ia_a;
  float ib_a;
  float ic_a;
};

struct SlowAnalogSample {
  uint32_t timestamp_ticks;
  uint16_t raw_vbat;
  uint16_t raw_board_ntc;
  uint16_t raw_stator_ntc;
  float vbat_v;
  float board_ntc_v;
  float stator_ntc_v;
};

void Init();
void CalibrateOffsets(uint32_t sample_count);
// Software trigger path used only for offset calibration before PWM starts.
void StartInjectedConversion();
bool ReadInjected(CurrentSample* sample);
void ServiceRegular(uint32_t control_ticks);
bool ReadSlowAnalog(SlowAnalogSample* sample);

}  // namespace sipdrive::hal::adc_opamp
