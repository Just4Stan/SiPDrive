#pragma once

#include <stdint.h>

namespace sipdrive::config {

#ifndef SIPDRIVE_USE_PLL170
#define SIPDRIVE_USE_PLL170 1
#endif

#ifndef SIPDRIVE_SYS_CLOCK_HZ
#if SIPDRIVE_USE_PLL170
#define SIPDRIVE_SYS_CLOCK_HZ 170000000U
#else
#define SIPDRIVE_SYS_CLOCK_HZ 16000000U
#endif
#endif

#ifndef SIPDRIVE_APB2_PRESCALER
#define SIPDRIVE_APB2_PRESCALER 1U
#endif

#ifndef SIPDRIVE_APB1_PRESCALER
#define SIPDRIVE_APB1_PRESCALER 1U
#endif

#ifndef SIPDRIVE_CONTROL_LOOP_HZ
#define SIPDRIVE_CONTROL_LOOP_HZ 40000U
#endif

#ifndef SIPDRIVE_PWM_FREQUENCY_HZ
#define SIPDRIVE_PWM_FREQUENCY_HZ 40000U
#endif

#ifndef SIPDRIVE_TIM1_DEADTIME_NS
#define SIPDRIVE_TIM1_DEADTIME_NS 250U
#endif

#ifndef SIPDRIVE_ADC_SAMPLE_OFFSET_TICKS
#define SIPDRIVE_ADC_SAMPLE_OFFSET_TICKS 0
#endif

#ifndef SIPDRIVE_REGULAR_ADC_RATE_HZ
#define SIPDRIVE_REGULAR_ADC_RATE_HZ 1000U
#endif

// OPAMP1 internal output → ADC1_IN13, OPAMP2 internal output → ADC2_IN16
// (STM32G4 RM0440 Table 76/77, with OPAMPINTEN set).
#ifndef SIPDRIVE_ADC_IA_INJECTED_CHANNEL
#define SIPDRIVE_ADC_IA_INJECTED_CHANNEL 13
#endif

#ifndef SIPDRIVE_ADC_IB_INJECTED_CHANNEL
#define SIPDRIVE_ADC_IB_INJECTED_CHANNEL 16
#endif

#ifndef SIPDRIVE_ENCODER_CPR
#define SIPDRIVE_ENCODER_CPR 16384U
#endif

// MT6701 MODE pin level:
//   0 = ABZ mode on pins 6/7/8
//   1 = SSI/I2C mode on pins 6/7/8
#ifndef SIPDRIVE_MT6701_MODE_HIGH
#define SIPDRIVE_MT6701_MODE_HIGH 1
#endif

#ifndef SIPDRIVE_DEBUG_TIMING
#define SIPDRIVE_DEBUG_TIMING 0
#endif

constexpr uint32_t kSysClockHz = static_cast<uint32_t>(SIPDRIVE_SYS_CLOCK_HZ);

constexpr uint32_t kApb2Prescaler = static_cast<uint32_t>(SIPDRIVE_APB2_PRESCALER);
constexpr uint32_t kApb1Prescaler = static_cast<uint32_t>(SIPDRIVE_APB1_PRESCALER);
constexpr uint32_t kTim1ClockHz =
    (kApb2Prescaler == 1U) ? kSysClockHz : ((kSysClockHz / kApb2Prescaler) * 2U);
constexpr uint32_t kTim3ClockHz =
    (kApb1Prescaler == 1U) ? kSysClockHz : ((kSysClockHz / kApb1Prescaler) * 2U);
constexpr uint32_t kTim4ClockHz = kTim3ClockHz;
constexpr uint32_t kControlLoopHz = static_cast<uint32_t>(SIPDRIVE_CONTROL_LOOP_HZ);
constexpr float kControlPeriodSec = 1.0f / static_cast<float>(kControlLoopHz);

constexpr uint32_t kPwmFrequencyHz = static_cast<uint32_t>(SIPDRIVE_PWM_FREQUENCY_HZ);
constexpr float kDutyMin = 0.02f;
constexpr float kDutyMax = 0.98f;
constexpr uint32_t kTim1DeadtimeNs = static_cast<uint32_t>(SIPDRIVE_TIM1_DEADTIME_NS);
constexpr int32_t kAdcSampleOffsetTicks =
    static_cast<int32_t>(SIPDRIVE_ADC_SAMPLE_OFFSET_TICKS);

constexpr float kBusVoltageV = 24.0f;
constexpr float kShuntOhm = 0.002f;           // SME08A1FR002T (2 milliohm).
constexpr float kOpampGain = 10.0f;            // Differential: Rf/Rin = 15k/1.5k.
constexpr uint8_t kOpampPgaGainBits = 0x04U;   // Not used: OPAMPs run in standalone mode.
constexpr float kAdcReferenceV = 3.3f;
constexpr uint16_t kAdcFullScale = 4095U;
constexpr uint32_t kCurrentOffsetSamples = 1024U;
constexpr uint32_t kRegularAdcRateHz = static_cast<uint32_t>(SIPDRIVE_REGULAR_ADC_RATE_HZ);
constexpr uint32_t kRegularAdcPeriodTicks =
    (kRegularAdcRateHz == 0U) ? 1U : ((kControlLoopHz / kRegularAdcRateHz) > 0U
                                         ? (kControlLoopHz / kRegularAdcRateHz)
                                         : 1U);

// TBD: these must be set from real board routing once OPAMP/ADC paths are finalized.
constexpr int32_t kAdcIaInjectedChannel =
    static_cast<int32_t>(SIPDRIVE_ADC_IA_INJECTED_CHANNEL);
constexpr int32_t kAdcIbInjectedChannel =
    static_cast<int32_t>(SIPDRIVE_ADC_IB_INJECTED_CHANNEL);
constexpr uint32_t kAdcInjectedSampleTimeBits = 4U;  // 47.5 ADC cycles.
constexpr uint32_t kAdcRegularSampleTimeBits = 6U;   // 247.5 ADC cycles.
constexpr uint8_t kAdcVbatChannel = 6U;              // PC0 -> ADC12_IN6 (ADC1 regular).
constexpr uint8_t kAdcBoardNtcChannel = 7U;          // PC1 -> ADC12_IN7.
constexpr uint8_t kAdcStatorNtcChannel = 8U;         // PC2 -> ADC12_IN8.
constexpr float kVbatDividerGain = 11.0f;            // 100k/10k divider -> (Rtop + Rbot) / Rbot.

// Placeholder pending MT6701 ABI configuration and timer x4 decode selection.
constexpr uint32_t kEncoderCountsPerRev = static_cast<uint32_t>(SIPDRIVE_ENCODER_CPR);
constexpr bool kMt6701ModeHigh = (SIPDRIVE_MT6701_MODE_HIGH != 0);
constexpr uint32_t kMotorPolePairs = 7U;

constexpr uint32_t kCanCommandId = 0x101U;
constexpr uint32_t kCanTelemetryId = 0x181U;
constexpr uint32_t kHeartbeatTicks = 4000U;

constexpr float kIdKp = 0.30f;
constexpr float kIdKi = 200.0f;
constexpr float kIqKp = 0.30f;
constexpr float kIqKi = 200.0f;

}  // namespace sipdrive::config
