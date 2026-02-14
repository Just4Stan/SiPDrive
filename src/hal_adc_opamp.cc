#include "hal_adc_opamp.h"

#include "config.h"
#include "hal_gpio.h"
#include "stm32g431xx.h"

namespace sipdrive::hal::adc_opamp {
namespace {

uint16_t g_offset_ia = config::kAdcFullScale / 2U;
uint16_t g_offset_ib = config::kAdcFullScale / 2U;
uint32_t g_sample_counter = 0U;
bool g_channels_configured = false;
bool g_adc_enabled = false;
uint8_t g_ia_channel = 0U;
uint8_t g_ib_channel = 0U;
uint32_t g_last_regular_ticks = 0U;
bool g_regular_valid = false;
SlowAnalogSample g_regular_sample = {};

enum class InjectedTriggerMode : uint8_t {
  kSoftware = 0,
  kTim1Trgo2 = 1,
};

InjectedTriggerMode g_trigger_mode = InjectedTriggerMode::kTim1Trgo2;

void ShortDelay() {
  // RM + datasheet require ADC regulator startup delay after ADVREGEN=1.
  // 5000 NOPs is safely above tADCVREG_STUP max (20 us) at 170 MHz.
  for (volatile uint32_t i = 0; i < 5000U; ++i) {
    __NOP();
  }
}

void EnableSingleAdc(ADC_TypeDef* adc) {
  if ((adc->CR & ADC_CR_ADEN) != 0U) {
    return;
  }

  adc->CR &= ~ADC_CR_DEEPPWD;
  adc->CR |= ADC_CR_ADVREGEN;
  ShortDelay();

  adc->CR |= ADC_CR_ADCAL;
  while ((adc->CR & ADC_CR_ADCAL) != 0U) {}

  adc->ISR = ADC_ISR_ADRDY;
  adc->CR |= ADC_CR_ADEN;
  while ((adc->ISR & ADC_ISR_ADRDY) == 0U) {}
}

void ConfigureSampleTime(ADC_TypeDef* adc, uint8_t channel, uint32_t sample_bits) {
  const uint32_t smp = (sample_bits & 0x7U);
  if (channel <= 9U) {
    const uint32_t shift = static_cast<uint32_t>(channel) * 3U;
    adc->SMPR1 = (adc->SMPR1 & ~(0x7UL << shift)) | (smp << shift);
  } else if (channel <= 18U) {
    const uint32_t shift = static_cast<uint32_t>(channel - 10U) * 3U;
    adc->SMPR2 = (adc->SMPR2 & ~(0x7UL << shift)) | (smp << shift);
  }
}

bool IsValidChannel(int32_t channel) {
  return channel >= 0 && channel <= 18;
}

void ConfigureAnalogPins() {
  gpio::EnablePort(GPIOC);
  gpio::ConfigureAnalog(GPIOC, 0U);  // VBAT_SENSE
  gpio::ConfigureAnalog(GPIOC, 1U);  // BOARD_NTC
  gpio::ConfigureAnalog(GPIOC, 2U);  // STATOR_NTC
}

void ConfigureInjectedSequence(ADC_TypeDef* adc, uint8_t channel, InjectedTriggerMode mode) {
  const uint32_t jexten = (mode == InjectedTriggerMode::kTim1Trgo2) ? 1U : 0U;

  // RM Interconnect 19, Table 67: ADC1/2 injected trigger selection value 8
  // maps to TIM1_TRGO2. Do not change this to TIM1_TRGO (0) or TIM1_CC4 (1).
  // RM JEXTEN encoding: 01 = rising edge, 00 = external trigger disabled.
  // Keep JQDIS=1: injected queue disabled. With JQDIS=1 both hardware and
  // software trigger detection are available; calibration/runtime switch is
  // controlled by JEXTEN + JADSTART usage.
  adc->CFGR |= ADC_CFGR_JQDIS;

  // Injected sequencer kept at 1 conversion for the 40kHz current loop.
  // Any Vbus/temperature channels should be added in regular conversions at
  // lower rate, not in this injected path.
  adc->JSQR =
      (0U << ADC_JSQR_JL_Pos) |
      (8U << ADC_JSQR_JEXTSEL_Pos) |
      (jexten << ADC_JSQR_JEXTEN_Pos) |
      (static_cast<uint32_t>(channel) << ADC_JSQR_JSQ1_Pos);
}

void ConfigureInjectedChannel(ADC_TypeDef* adc, uint8_t channel, InjectedTriggerMode mode) {
  ConfigureSampleTime(adc, channel, config::kAdcInjectedSampleTimeBits);
  ConfigureInjectedSequence(adc, channel, mode);
}

void SetTriggerMode(InjectedTriggerMode mode) {
  if (!g_channels_configured) {
    return;
  }

  ConfigureInjectedSequence(ADC1, g_ia_channel, mode);
  ConfigureInjectedSequence(ADC2, g_ib_channel, mode);
  ADC1->ISR = ADC_ISR_JEOS | ADC_ISR_JEOC;
  ADC2->ISR = ADC_ISR_JEOS | ADC_ISR_JEOC;
  g_trigger_mode = mode;
}

void ConfigureOpampPga(OPAMP_TypeDef* opamp) {
  // VMSEL=10 selects PGA mode (VMSEL=11 would be follower, ignoring gain).
  opamp->CSR =
      OPAMP_CSR_OPAMPINTEN |
      OPAMP_CSR_VMSEL_1 |
      (static_cast<uint32_t>(config::kOpampPgaGainBits) << OPAMP_CSR_PGGAIN_Pos) |
      OPAMP_CSR_HIGHSPEEDEN |
      OPAMP_CSR_OPAMPxEN;
}

bool ConversionDone() {
  return ((ADC1->ISR & ADC_ISR_JEOS) != 0U) && ((ADC2->ISR & ADC_ISR_JEOS) != 0U);
}

float ConvertRawToAmps(uint16_t raw, uint16_t offset) {
  const float volts =
      (static_cast<float>(static_cast<int32_t>(raw) - static_cast<int32_t>(offset)) *
       config::kAdcReferenceV) /
      static_cast<float>(config::kAdcFullScale);
  return volts / (config::kShuntOhm * config::kOpampGain);
}

float ConvertRawToVolts(uint16_t raw) {
  return (static_cast<float>(raw) * config::kAdcReferenceV) /
         static_cast<float>(config::kAdcFullScale);
}

bool WaitForFlag(volatile uint32_t* reg, uint32_t mask) {
  uint32_t timeout = 20000U;
  while (((*reg & mask) == 0U) && timeout > 0U) {
    --timeout;
  }
  return ((*reg & mask) != 0U);
}

bool ReadRegularSingle(ADC_TypeDef* adc, uint8_t channel, uint16_t* value) {
  if (value == nullptr) {
    return false;
  }

  ConfigureSampleTime(adc, channel, config::kAdcRegularSampleTimeBits);
  adc->SQR1 = (static_cast<uint32_t>(channel) << ADC_SQR1_SQ1_Pos);
  adc->ISR = ADC_ISR_EOC | ADC_ISR_EOS;
  adc->CR |= ADC_CR_ADSTART;
  if (!WaitForFlag(&adc->ISR, ADC_ISR_EOC)) {
    return false;
  }
  *value = static_cast<uint16_t>(adc->DR & ADC_DR_RDATA);
  return true;
}

bool ReadRegularAdc2Pair(uint16_t* board_ntc, uint16_t* stator_ntc) {
  if (board_ntc == nullptr || stator_ntc == nullptr) {
    return false;
  }

  ConfigureSampleTime(ADC2, config::kAdcBoardNtcChannel, config::kAdcRegularSampleTimeBits);
  ConfigureSampleTime(ADC2, config::kAdcStatorNtcChannel, config::kAdcRegularSampleTimeBits);

  ADC2->SQR1 =
      (1U << ADC_SQR1_L_Pos) |
      (static_cast<uint32_t>(config::kAdcBoardNtcChannel) << ADC_SQR1_SQ1_Pos) |
      (static_cast<uint32_t>(config::kAdcStatorNtcChannel) << ADC_SQR1_SQ2_Pos);
  ADC2->ISR = ADC_ISR_EOC | ADC_ISR_EOS;
  ADC2->CR |= ADC_CR_ADSTART;

  if (!WaitForFlag(&ADC2->ISR, ADC_ISR_EOC)) {
    return false;
  }
  *board_ntc = static_cast<uint16_t>(ADC2->DR & ADC_DR_RDATA);

  if (!WaitForFlag(&ADC2->ISR, ADC_ISR_EOC)) {
    return false;
  }
  *stator_ntc = static_cast<uint16_t>(ADC2->DR & ADC_DR_RDATA);
  return true;
}

}  // namespace

void Init() {
  g_channels_configured = false;
  g_adc_enabled = false;
  g_trigger_mode = InjectedTriggerMode::kTim1Trgo2;
  g_regular_valid = false;
  g_last_regular_ticks = 0U;

  RCC->AHB2ENR |= RCC_AHB2ENR_ADC12EN;
  RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
  (void)RCC->AHB2ENR;

  ConfigureAnalogPins();

  // CKMODE=11: synchronous HCLK/4 = 42.5 MHz (170/4). /1 would be 170 MHz,
  // exceeding the STM32G431 ADC characterization limit of ~60 MHz.
  ADC12_COMMON->CCR = (ADC12_COMMON->CCR & ~ADC_CCR_CKMODE) |
                      (ADC_CCR_CKMODE_0 | ADC_CCR_CKMODE_1);

  ConfigureOpampPga(OPAMP1);
  ConfigureOpampPga(OPAMP2);

  const bool injected_channels_valid =
      IsValidChannel(config::kAdcIaInjectedChannel) &&
      IsValidChannel(config::kAdcIbInjectedChannel);
  if (injected_channels_valid) {
    g_ia_channel = static_cast<uint8_t>(config::kAdcIaInjectedChannel);
    g_ib_channel = static_cast<uint8_t>(config::kAdcIbInjectedChannel);

    ConfigureInjectedChannel(ADC1, g_ia_channel, InjectedTriggerMode::kTim1Trgo2);
    ConfigureInjectedChannel(ADC2, g_ib_channel, InjectedTriggerMode::kTim1Trgo2);
  }

  EnableSingleAdc(ADC1);
  EnableSingleAdc(ADC2);
  g_adc_enabled = true;

  ADC1->ISR = ADC_ISR_JEOS | ADC_ISR_JEOC;
  ADC2->ISR = ADC_ISR_JEOS | ADC_ISR_JEOC;
  g_channels_configured = injected_channels_valid;
}

void CalibrateOffsets(uint32_t sample_count) {
  if (!g_channels_configured) {
    return;
  }

  // Calibration is software-triggered only: disable external injected trigger.
  SetTriggerMode(InjectedTriggerMode::kSoftware);

  uint64_t sum_ia = 0U;
  uint64_t sum_ib = 0U;
  uint32_t valid = 0U;

  if (sample_count == 0U) {
    sample_count = 1U;
  }

  for (uint32_t i = 0; i < sample_count; ++i) {
    StartInjectedConversion();

    uint32_t timeout = 10000U;
    while (!ConversionDone() && timeout > 0U) {
      --timeout;
    }
    if (!ConversionDone()) {
      continue;
    }

    const uint16_t ia = static_cast<uint16_t>(ADC1->JDR1 & ADC_JDR1_JDATA);
    const uint16_t ib = static_cast<uint16_t>(ADC2->JDR1 & ADC_JDR1_JDATA);

    ADC1->ISR = ADC_ISR_JEOS | ADC_ISR_JEOC;
    ADC2->ISR = ADC_ISR_JEOS | ADC_ISR_JEOC;

    sum_ia += ia;
    sum_ib += ib;
    ++valid;
  }

  if (valid == 0U) {
    SetTriggerMode(InjectedTriggerMode::kTim1Trgo2);
    return;
  }

  g_offset_ia = static_cast<uint16_t>(sum_ia / valid);
  g_offset_ib = static_cast<uint16_t>(sum_ib / valid);
  // Runtime current loop is hardware-triggered from TIM1_TRGO2 rising edge.
  SetTriggerMode(InjectedTriggerMode::kTim1Trgo2);
}

void StartInjectedConversion() {
  if (!g_channels_configured || !g_adc_enabled) {
    return;
  }
  if (g_trigger_mode != InjectedTriggerMode::kSoftware) {
    return;
  }
  ADC1->CR |= ADC_CR_JADSTART;
  ADC2->CR |= ADC_CR_JADSTART;
}

bool ReadInjected(CurrentSample* sample) {
  if (sample == nullptr) {
    return false;
  }
  if (!g_channels_configured || !g_adc_enabled) {
    return false;
  }

  if (!ConversionDone()) {
    return false;
  }

  const uint16_t raw_ia = static_cast<uint16_t>(ADC1->JDR1 & ADC_JDR1_JDATA);
  const uint16_t raw_ib = static_cast<uint16_t>(ADC2->JDR1 & ADC_JDR1_JDATA);

  ADC1->ISR = ADC_ISR_JEOS | ADC_ISR_JEOC;
  ADC2->ISR = ADC_ISR_JEOS | ADC_ISR_JEOC;

  const float ia = ConvertRawToAmps(raw_ia, g_offset_ia);
  const float ib = ConvertRawToAmps(raw_ib, g_offset_ib);

  sample->timestamp_ticks = g_sample_counter++;
  sample->raw_ia = raw_ia;
  sample->raw_ib = raw_ib;
  sample->ia_a = ia;
  sample->ib_a = ib;
  sample->ic_a = -(ia + ib);
  return true;
}

void ServiceRegular(uint32_t control_ticks) {
  if (!g_adc_enabled) {
    return;
  }
  if ((control_ticks - g_last_regular_ticks) < config::kRegularAdcPeriodTicks) {
    return;
  }
  g_last_regular_ticks = control_ticks;

  SlowAnalogSample sample;
  sample.timestamp_ticks = control_ticks;
  sample.raw_vbat = 0U;
  sample.raw_board_ntc = 0U;
  sample.raw_stator_ntc = 0U;
  sample.vbat_v = 0.0f;
  sample.board_ntc_v = 0.0f;
  sample.stator_ntc_v = 0.0f;

  if (!ReadRegularSingle(ADC1, config::kAdcVbatChannel, &sample.raw_vbat)) {
    return;
  }
  if (!ReadRegularAdc2Pair(&sample.raw_board_ntc, &sample.raw_stator_ntc)) {
    return;
  }

  sample.vbat_v = ConvertRawToVolts(sample.raw_vbat) * config::kVbatDividerGain;
  sample.board_ntc_v = ConvertRawToVolts(sample.raw_board_ntc);
  sample.stator_ntc_v = ConvertRawToVolts(sample.raw_stator_ntc);

  g_regular_sample = sample;
  g_regular_valid = true;
}

bool ReadSlowAnalog(SlowAnalogSample* sample) {
  if (sample == nullptr || !g_regular_valid) {
    return false;
  }
  *sample = g_regular_sample;
  return true;
}

}  // namespace sipdrive::hal::adc_opamp
