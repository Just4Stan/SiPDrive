#include "hal_tim1_pwm.h"

#include "config.h"
#include "hal_gpio.h"
#include "stm32g431xx.h"

namespace sipdrive::hal::tim1_pwm {
namespace {

#ifndef SIPDRIVE_USE_BKIN
#define SIPDRIVE_USE_BKIN 0
#endif

#ifndef SIPDRIVE_DEBUG_TIMING
#define SIPDRIVE_DEBUG_TIMING 0
#endif

UpdateCallback g_update_callback = nullptr;
volatile bool g_fault_latched = false;

constexpr uint8_t kTim1Af = 2U;
constexpr uint8_t kTim1BkinAf = 2U;

uint32_t DutyToCcr(float duty) {
  if (duty < config::kDutyMin) {
    duty = config::kDutyMin;
  }
  if (duty > config::kDutyMax) {
    duty = config::kDutyMax;
  }
  const uint32_t arr = TIM1->ARR;
  return static_cast<uint32_t>(duty * static_cast<float>(arr));
}

uint32_t DeadtimeTicksFromDtg(uint8_t dtg) {
  if ((dtg & 0x80U) == 0U) {
    return dtg;
  }
  if ((dtg & 0xC0U) == 0x80U) {
    return (64U + (dtg & 0x3FU)) * 2U;
  }
  if ((dtg & 0xE0U) == 0xC0U) {
    return (32U + (dtg & 0x1FU)) * 8U;
  }
  return (32U + (dtg & 0x1FU)) * 16U;
}

uint8_t EncodeTimDeadtimeNs(uint32_t deadtime_ns) {
  if (deadtime_ns == 0U) {
    return 0U;
  }

  const uint64_t target_ticks =
      ((static_cast<uint64_t>(deadtime_ns) * config::kTim1ClockHz) + 999999999ULL) /
      1000000000ULL;

  for (uint32_t dtg = 0U; dtg <= 0xFFU; ++dtg) {
    if (DeadtimeTicksFromDtg(static_cast<uint8_t>(dtg)) >= target_ticks) {
      return static_cast<uint8_t>(dtg);
    }
  }
  return 0xFFU;
}

uint32_t ComputeAdcSampleCcr4(uint32_t arr) {
  int32_t ccr = static_cast<int32_t>(arr / 2U) + config::kAdcSampleOffsetTicks;
  if (ccr < 0) {
    ccr = 0;
  } else if (ccr > static_cast<int32_t>(arr)) {
    ccr = static_cast<int32_t>(arr);
  }
  return static_cast<uint32_t>(ccr);
}

#if SIPDRIVE_DEBUG_TIMING
GPIO_TypeDef* const kDebugUpdatePort = GPIOC;
constexpr uint8_t kDebugUpdatePin = 13U;

void DebugTimingInit() {
  gpio::EnablePort(kDebugUpdatePort);
  gpio::ConfigureOutput(kDebugUpdatePort, kDebugUpdatePin, true);
  gpio::Write(kDebugUpdatePort, kDebugUpdatePin, false);
}

inline void DebugTimingSetUpdate(bool high) {
  gpio::Write(kDebugUpdatePort, kDebugUpdatePin, high);
}
#else
void DebugTimingInit() {}
inline void DebugTimingSetUpdate(bool) {}
#endif

}  // namespace

UpdateCallback GetUpdateCallbackForIsr() {
  return g_update_callback;
}

void SetDebugTimingForIsr(bool high) {
  DebugTimingSetUpdate(high);
}

void SetFaultLatchForIsr() {
  g_fault_latched = true;
}

uint32_t AutoReload() {
  return (config::kTim1ClockHz / (2U * config::kPwmFrequencyHz)) - 1U;
}

void Init() {
  gpio::EnablePort(GPIOE);
  DebugTimingInit();

  // STSPIN32G4 SiP TIM1 mapping on GPIOE:
  // PE8  AF2 TIM1_CH1N -> INL1
  // PE9  AF2 TIM1_CH1  -> INH1
  // PE10 AF2 TIM1_CH2N -> INL2
  // PE11 AF2 TIM1_CH2  -> INH2
  // PE12 AF2 TIM1_CH3N -> INL3
  // PE13 AF2 TIM1_CH3  -> INH3
  // PE14 AF2 TIM1_CH4 capability exists; this build keeps PE14 as READY GPIO.
  // TIM1 CH4 is still used internally as ADC timing marker (CC4 output disabled).
  // PE15 input nFAULT (or AF2 TIM1_BKIN when SIPDRIVE_USE_BKIN=1).
  gpio::ConfigureAlternate(GPIOE, 8U, kTim1Af, gpio::Pull::kNone);
  gpio::ConfigureAlternate(GPIOE, 9U, kTim1Af, gpio::Pull::kNone);
  gpio::ConfigureAlternate(GPIOE, 10U, kTim1Af, gpio::Pull::kNone);
  gpio::ConfigureAlternate(GPIOE, 11U, kTim1Af, gpio::Pull::kNone);
  gpio::ConfigureAlternate(GPIOE, 12U, kTim1Af, gpio::Pull::kNone);
  gpio::ConfigureAlternate(GPIOE, 13U, kTim1Af, gpio::Pull::kNone);

  // READY input from STSPIN32G4.
  gpio::ConfigureInput(GPIOE, 14U, gpio::Pull::kUp);
#if SIPDRIVE_USE_BKIN
  // Optional hardware break: PE15 AF2 TIM1_BKIN, active-low from STSPIN nFAULT.
  gpio::ConfigureAlternate(GPIOE, 15U, kTim1BkinAf, gpio::Pull::kUp);
#else
  // PE15 input nFAULT (active-low), polled in update ISR.
  gpio::ConfigureInput(GPIOE, 15U, gpio::Pull::kUp);
#endif

  RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;
  (void)RCC->APB2ENR;

  TIM1->CR1 = TIM_CR1_ARPE | TIM_CR1_CMS_0;
  TIM1->PSC = 0U;
  TIM1->ARR = AutoReload();
  // In center-aligned mode, UIF fires at both overflow AND underflow.
  // RCR=1 ensures one update event per complete PWM cycle (not 2x).
  TIM1->RCR = 1U;

  const uint32_t midpoint = TIM1->ARR / 2U;
  TIM1->CCR1 = midpoint;
  TIM1->CCR2 = midpoint;
  TIM1->CCR3 = midpoint;
  TIM1->CCR4 = ComputeAdcSampleCcr4(TIM1->ARR);

  TIM1->CCMR1 =
      TIM_CCMR1_OC1PE |
      TIM_CCMR1_OC1M_1 |
      TIM_CCMR1_OC1M_2 |
      TIM_CCMR1_OC2PE |
      TIM_CCMR1_OC2M_1 |
      TIM_CCMR1_OC2M_2;

  TIM1->CCMR2 =
      TIM_CCMR2_OC3PE |
      TIM_CCMR2_OC3M_1 |
      TIM_CCMR2_OC3M_2 |
      TIM_CCMR2_OC4PE |
      TIM_CCMR2_OC4M_1 |
      TIM_CCMR2_OC4M_2;

  TIM1->CCER =
      TIM_CCER_CC1E | TIM_CCER_CC1NE |
      TIM_CCER_CC2E | TIM_CCER_CC2NE |
      TIM_CCER_CC3E | TIM_CCER_CC3NE;

  TIM1->BDTR =
      (static_cast<uint32_t>(EncodeTimDeadtimeNs(config::kTim1DeadtimeNs)) & TIM_BDTR_DTG) |
      TIM_BDTR_OSSR |
      TIM_BDTR_OSSI
#if SIPDRIVE_USE_BKIN
      | TIM_BDTR_BKE  // nFAULT is active-low; BKP=0 means active-low input.
#endif
      ;

  // TRGO2 must be OC4REF (MMS2=0b0111) so ADC trigger is phase-locked to PWM
  // midpoint, not to update (which can shift if ARR preload/UG timing changes).
  // CH4 is a timing marker only; CC4 output is disabled in CCER.
  TIM1->CR2 = (TIM1->CR2 & ~TIM_CR2_MMS2) |
              (TIM_CR2_MMS2_0 | TIM_CR2_MMS2_1 | TIM_CR2_MMS2_2);

  TIM1->EGR = TIM_EGR_UG;
  TIM1->DIER |= TIM_DIER_UIE;

  NVIC_SetPriority(TIM1_UP_TIM16_IRQn, 1U);
  NVIC_EnableIRQ(TIM1_UP_TIM16_IRQn);

  DisablePowerStage();
}

void Start() {
  TIM1->SR = 0U;
  TIM1->CR1 |= TIM_CR1_CEN;
}

void RegisterUpdateCallback(UpdateCallback callback) {
  g_update_callback = callback;
}

void SetDutyCycles(const DutyCycles& duty) {
  TIM1->CCR1 = DutyToCcr(duty.phase_a);
  TIM1->CCR2 = DutyToCcr(duty.phase_b);
  TIM1->CCR3 = DutyToCcr(duty.phase_c);
}

void DisablePowerStage() {
  TIM1->BDTR &= ~TIM_BDTR_MOE;
  TIM1->CCR1 = 0U;
  TIM1->CCR2 = 0U;
  TIM1->CCR3 = 0U;
}

void EnablePowerStage() {
  if (FaultActive()) {
    g_fault_latched = true;
    DisablePowerStage();
    return;
  }
  TIM1->BDTR |= TIM_BDTR_MOE;
}

bool FaultActive() {
  // nFAULT on PE15 is active-low.
  return !gpio::Read(GPIOE, 15U);
}

bool FaultLatched() {
  return g_fault_latched;
}

void ClearFaultLatch() {
  g_fault_latched = false;
}

}  // namespace sipdrive::hal::tim1_pwm

extern "C" void TIM1_UP_TIM16_IRQHandler(void) {
  if ((TIM1->SR & TIM_SR_UIF) == 0U) {
    return;
  }

  TIM1->SR = ~TIM_SR_UIF;
  sipdrive::hal::tim1_pwm::SetDebugTimingForIsr(true);

#if SIPDRIVE_USE_BKIN
  if ((TIM1->SR & TIM_SR_BIF) != 0U) {
    TIM1->SR = ~TIM_SR_BIF;
    sipdrive::hal::tim1_pwm::SetFaultLatchForIsr();
    sipdrive::hal::tim1_pwm::DisablePowerStage();
    sipdrive::hal::tim1_pwm::SetDebugTimingForIsr(false);
    __DSB();
    return;
  }
#endif

  if (sipdrive::hal::tim1_pwm::FaultActive()) {
    sipdrive::hal::tim1_pwm::SetFaultLatchForIsr();
    sipdrive::hal::tim1_pwm::DisablePowerStage();
    sipdrive::hal::tim1_pwm::SetDebugTimingForIsr(false);
    __DSB();
    return;
  }

  // Execute the 40kHz current loop hook.
  if (sipdrive::hal::tim1_pwm::UpdateCallback callback =
          sipdrive::hal::tim1_pwm::GetUpdateCallbackForIsr();
      callback != nullptr) {
    callback();
  }
  sipdrive::hal::tim1_pwm::SetDebugTimingForIsr(false);
}
