#include "hal_qei.h"

#include "config.h"
#include "hal_gpio.h"
#include "stm32g431xx.h"
#include "math_compat.h"

namespace sipdrive::hal::qei {

// Calibration data (accessed from both anonymous namespace and public API).
static float g_elec_offset_rad = 0.0f;
static bool g_calib_valid = false;
static uint16_t g_calib_table[persistent_config::kCalibTableSize] = {};
static uint32_t g_calib_table_size = 0U;

namespace {

constexpr float kPi = 3.14159265359f;
constexpr float kTwoPi = 6.283185307179586f;
// TIM4 alternate-function mapping (STM32G431 datasheet Table 13):
//   PB7 -> TIM4_CH2 = AF2
//   PB6 -> TIM4_CH1 = AF3  (NOT AF2 — AF2 is undefined on PB6)
// PB6/PB7 have NO SPI alternate functions, so SSI mode uses
// bit-banged GPIO (adequate at 170 MHz for MT6701's 14-bit frame).
constexpr uint8_t kTim4Ch2Af = 2U;   // PB7 -> TIM4_CH2.
constexpr uint8_t kTim4Ch1Af = 3U;   // PB6 -> TIM4_CH1.

// MT6701 QFN-16 signal mapping used in this project.
constexpr uint8_t kMt6701DoPin = 7U;    // U2 pin6: A/DO  -> PB7
constexpr uint8_t kMt6701ClkPin = 6U;   // U2 pin7: B/CLK -> PB6
constexpr uint8_t kMt6701CsnPin = 4U;   // U2 pin8: Z/CSN -> PB4
constexpr uint8_t kMt6701ModePin = 5U;  // U2 pin14: MODE -> PB5

enum class EncoderInterface : uint8_t {
  kAbz = 0,
  kSsi = 1,
};

EncoderInterface g_interface = EncoderInterface::kAbz;

uint16_t g_last_count = 0U;
int32_t g_accumulated_counts = 0;
float g_velocity_rad_s = 0.0f;
volatile bool g_index_seen = false;

bool g_ssi_valid = false;
uint16_t g_ssi_last_code = 0U;

float CountsToRadians(int32_t counts) {
  return static_cast<float>(counts) *
         (kTwoPi / static_cast<float>(config::kEncoderCountsPerRev));
}

float WrapPi(float angle) {
  angle = fmodf(angle, kTwoPi);
  if (angle > kPi) {
    angle -= kTwoPi;
  } else if (angle < -kPi) {
    angle += kTwoPi;
  }
  return angle;
}

void ModeSwitchDelay() {
  // Allow MT6701 MODE strap state to settle before interface activity.
  for (volatile uint32_t i = 0; i < 2000U; ++i) {
    __NOP();
  }
}

void InitIndexExti() {
  RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
  (void)RCC->APB2ENR;

  SYSCFG->EXTICR[1] =
      (SYSCFG->EXTICR[1] & ~SYSCFG_EXTICR2_EXTI4) | SYSCFG_EXTICR2_EXTI4_PB;
  EXTI->RTSR1 |= EXTI_RTSR1_RT4;
  EXTI->FTSR1 &= ~EXTI_FTSR1_FT4;
  EXTI->IMR1 |= EXTI_IMR1_IM4;
  EXTI->PR1 = EXTI_PR1_PIF4;

  NVIC_SetPriority(EXTI4_IRQn, 2U);
  NVIC_EnableIRQ(EXTI4_IRQn);
}

void InitAbzInterface() {
  // PB7 -> TIM4_CH2 AF2 (MT6701 A), PB6 -> TIM4_CH1 AF3 (MT6701 B),
  // PB4 -> Z index (EXTI4).
  // MT6701 A/B are swapped relative to TIM4 CH1/CH2, reversing the
  // count direction.  Calibration handles the resulting sign.
  gpio::ConfigureAlternate(GPIOB, 7U, kTim4Ch2Af, gpio::Pull::kUp);
  gpio::ConfigureAlternate(GPIOB, 6U, kTim4Ch1Af, gpio::Pull::kUp);
  gpio::ConfigureInput(GPIOB, 4U, gpio::Pull::kUp);

  RCC->APB1ENR1 |= RCC_APB1ENR1_TIM4EN;
  (void)RCC->APB1ENR1;

  TIM4->PSC = 0U;
  TIM4->ARR = 0xFFFFU;
  TIM4->CNT = 0U;

  TIM4->CCMR1 = TIM_CCMR1_CC1S_0 | TIM_CCMR1_CC2S_0;
  TIM4->CCER = 0U;
  TIM4->SMCR = TIM_SMCR_SMS_0 | TIM_SMCR_SMS_1;
  TIM4->CR1 = TIM_CR1_CEN;

  g_interface = EncoderInterface::kAbz;
  g_last_count = 0U;
  InitIndexExti();
}

void InitSsiInterface() {
  // PB7 = DO input, PB6 = CLK output, PB4 = CSN output.
  gpio::ConfigureInput(GPIOB, kMt6701DoPin, gpio::Pull::kUp);

  gpio::ConfigureOutput(GPIOB, kMt6701ClkPin, true);
  gpio::Write(GPIOB, kMt6701ClkPin, true);  // SSI idle high.

  gpio::ConfigureOutput(GPIOB, kMt6701CsnPin, true);
  gpio::Write(GPIOB, kMt6701CsnPin, true);  // inactive high.

  g_interface = EncoderInterface::kSsi;
  g_ssi_valid = false;
}

bool ReadSsiAngleCode(uint16_t* angle_code) {
  if (angle_code == nullptr) {
    return false;
  }

  uint32_t raw = 0U;

  // Ensure idle state before transaction.
  GPIOB->BSRR = (1UL << kMt6701ClkPin);
  GPIOB->BSRR = (1UL << kMt6701CsnPin);

  // Start frame.
  GPIOB->BSRR = (1UL << (kMt6701CsnPin + 16U));

  // 24 clocks: DO shifts out D13..D0 then trailing bits.
  for (uint32_t i = 0U; i < 24U; ++i) {
    // Falling edge.
    GPIOB->BSRR = (1UL << (kMt6701ClkPin + 16U));

    const uint32_t bit = (GPIOB->IDR >> kMt6701DoPin) & 0x1U;
    raw = (raw << 1U) | bit;

    // Rising edge.
    GPIOB->BSRR = (1UL << kMt6701ClkPin);
  }

  // End frame.
  GPIOB->BSRR = (1UL << kMt6701CsnPin);

  // First 14 bits are angle D13..D0.
  *angle_code = static_cast<uint16_t>((raw >> 10U) & 0x3FFFU);
  return true;
}

void SampleSsi(float dt_s) {
  uint16_t code = 0U;
  if (!ReadSsiAngleCode(&code)) {
    return;
  }

  if (!g_ssi_valid) {
    g_ssi_last_code = code;
    g_accumulated_counts = static_cast<int32_t>(code);
    g_velocity_rad_s = 0.0f;
    g_ssi_valid = true;
    return;
  }

  int32_t delta =
      static_cast<int32_t>(code) - static_cast<int32_t>(g_ssi_last_code);

  // Wrap across 14-bit absolute range.
  constexpr int32_t kHalfSpan = 8192;
  constexpr int32_t kFullSpan = 16384;
  if (delta > kHalfSpan) {
    delta -= kFullSpan;
  } else if (delta < -kHalfSpan) {
    delta += kFullSpan;
  }

  g_ssi_last_code = code;
  g_accumulated_counts += delta;

  if (dt_s > 0.0f) {
    g_velocity_rad_s = CountsToRadians(delta) / dt_s;
  }
}

}  // namespace

void Init() {
  gpio::EnablePort(GPIOB);

  // Drive MODE from PB5 so firmware can select ABZ vs SSI wiring mode.
  gpio::ConfigureOutput(GPIOB, kMt6701ModePin, true);
  gpio::Write(GPIOB, kMt6701ModePin, config::kMt6701ModeHigh);
  ModeSwitchDelay();

  g_accumulated_counts = 0;
  g_velocity_rad_s = 0.0f;
  g_index_seen = false;

  if (config::kMt6701ModeHigh) {
    InitSsiInterface();
  } else {
    InitAbzInterface();
  }
}

void Sample(float dt_s) {
  if (g_interface == EncoderInterface::kSsi) {
    SampleSsi(dt_s);
    return;
  }

  const uint16_t count = static_cast<uint16_t>(TIM4->CNT);
  const int16_t delta = static_cast<int16_t>(count - g_last_count);
  g_last_count = count;
  g_accumulated_counts += static_cast<int32_t>(delta);

  if (dt_s > 0.0f) {
    g_velocity_rad_s = CountsToRadians(static_cast<int32_t>(delta)) / dt_s;
  }
}

int32_t GetPositionCounts() {
  return g_accumulated_counts;
}

float GetMechanicalAngleRad() {
  return CountsToRadians(GetPositionCounts());
}

float GetElectricalAngleRad() {
  float angle = GetMechanicalAngleRad() * static_cast<float>(config::kMotorPolePairs);

  if (g_calib_valid && g_calib_table_size > 0U) {
    // Apply calibration offset.
    angle += g_elec_offset_rad;

    // Apply compensation table.  Index by mechanical angle.
    const float mech = GetMechanicalAngleRad();
    float mech_pos = fmodf(mech, kTwoPi);
    if (mech_pos < 0.0f) { mech_pos += kTwoPi; }
    const float bin_f =
        mech_pos * static_cast<float>(g_calib_table_size) / kTwoPi;
    uint32_t bin = static_cast<uint32_t>(bin_f);
    if (bin >= g_calib_table_size) { bin = g_calib_table_size - 1U; }
    // Decode Q1.15 correction (stored as uint16_t holding signed int16_t).
    const float correction =
        static_cast<float>(static_cast<int16_t>(g_calib_table[bin])) *
        (kPi / 32768.0f);
    angle += correction;
  }

  return WrapPi(angle);
}

float GetVelocityRadPerSec() {
  return g_velocity_rad_s;
}

bool IndexSeen() {
  return g_index_seen;
}

bool GetPwmMechanicalAngleRad(float* angle_rad) {
  (void)angle_rad;
  return false;
}

void SetCalibration(float elec_offset_rad,
                    const uint16_t* table,
                    uint32_t table_size) {
  g_elec_offset_rad = elec_offset_rad;
  if (table != nullptr && table_size > 0U) {
    if (table_size > persistent_config::kCalibTableSize) {
      table_size = persistent_config::kCalibTableSize;
    }
    for (uint32_t i = 0; i < table_size; ++i) {
      g_calib_table[i] = table[i];
    }
    g_calib_table_size = table_size;
  } else {
    g_calib_table_size = 0U;
  }
  g_calib_valid = true;
}

void HandleIndexPulseForIsr() {
  if (g_interface != EncoderInterface::kAbz) {
    return;
  }

  TIM4->CNT = 0U;
  g_last_count = 0U;
  g_accumulated_counts = 0;
  g_velocity_rad_s = 0.0f;
  g_index_seen = true;
}

}  // namespace sipdrive::hal::qei

extern "C" void EXTI4_IRQHandler(void) {
  if ((EXTI->PR1 & EXTI_PR1_PIF4) == 0U) {
    return;
  }

  EXTI->PR1 = EXTI_PR1_PIF4;
  sipdrive::hal::qei::HandleIndexPulseForIsr();
}
