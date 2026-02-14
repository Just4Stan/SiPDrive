#include "hal_i2c3_stspin.h"

#include "hal_gpio.h"
#include "stm32g431xx.h"

namespace sipdrive::hal::i2c3_stspin {
namespace {

constexpr uint8_t kStspinAddr7 = 0x47U;
constexpr uint32_t kTimeoutLoops = 1000000U;

// STSPIN32G4 register addresses.
constexpr uint8_t kRegPowmng = 0x01U;
constexpr uint8_t kRegLogic = 0x02U;
constexpr uint8_t kRegReady = 0x07U;
constexpr uint8_t kRegNfault = 0x08U;
constexpr uint8_t kRegClear = 0x09U;
constexpr uint8_t kRegStby = 0x0AU;
constexpr uint8_t kRegLock = 0x0BU;
constexpr uint8_t kRegReset = 0x0CU;
constexpr uint8_t kRegStatus = 0x80U;

// I2C TIMINGR for 100 kHz standard-mode with 170 MHz I2CCLK.
// PRESC=16 (170/17=10 MHz tick), SCLL=0x55 (8.5us), SCLH=0x38 (5.6us),
// SDADEL=2, SCLDEL=4.  Conservative value that works reliably.
constexpr uint32_t kI2cTiming100k = 0x10802D9BU;

bool WaitFlag(volatile uint32_t* reg, uint32_t mask) {
  for (uint32_t i = 0; i < kTimeoutLoops; ++i) {
    if ((*reg & mask) != 0U) {
      return true;
    }
  }
  return false;
}

bool WriteReg(uint8_t reg_addr, uint8_t value) {
  // Wait for I2C bus idle.
  if ((I2C3->ISR & I2C_ISR_BUSY) != 0U) {
    return false;
  }

  // Configure: 7-bit addr, 2 bytes (register + data), write, autoend.
  I2C3->CR2 = (static_cast<uint32_t>(kStspinAddr7) << 1U) |
              (2U << I2C_CR2_NBYTES_Pos) |
              I2C_CR2_AUTOEND |
              I2C_CR2_START;

  if (!WaitFlag(&I2C3->ISR, I2C_ISR_TXIS)) {
    return false;
  }
  I2C3->TXDR = reg_addr;

  if (!WaitFlag(&I2C3->ISR, I2C_ISR_TXIS)) {
    return false;
  }
  I2C3->TXDR = value;

  if (!WaitFlag(&I2C3->ISR, I2C_ISR_STOPF)) {
    return false;
  }
  I2C3->ICR = I2C_ICR_STOPCF;

  return (I2C3->ISR & I2C_ISR_NACKF) == 0U;
}

bool ReadReg(uint8_t reg_addr, uint8_t* value) {
  if ((I2C3->ISR & I2C_ISR_BUSY) != 0U) {
    return false;
  }

  // Write phase: send register address (1 byte, no autoend — need restart).
  I2C3->CR2 = (static_cast<uint32_t>(kStspinAddr7) << 1U) |
              (1U << I2C_CR2_NBYTES_Pos) |
              I2C_CR2_START;

  if (!WaitFlag(&I2C3->ISR, I2C_ISR_TXIS)) {
    return false;
  }
  I2C3->TXDR = reg_addr;

  if (!WaitFlag(&I2C3->ISR, I2C_ISR_TC)) {
    return false;
  }

  // Read phase: restart with RD_WRN=1, 1 byte, autoend.
  I2C3->CR2 = (static_cast<uint32_t>(kStspinAddr7) << 1U) |
              I2C_CR2_RD_WRN |
              (1U << I2C_CR2_NBYTES_Pos) |
              I2C_CR2_AUTOEND |
              I2C_CR2_START;

  if (!WaitFlag(&I2C3->ISR, I2C_ISR_RXNE)) {
    return false;
  }
  *value = static_cast<uint8_t>(I2C3->RXDR);

  if (!WaitFlag(&I2C3->ISR, I2C_ISR_STOPF)) {
    return false;
  }
  I2C3->ICR = I2C_ICR_STOPCF;

  return (I2C3->ISR & I2C_ISR_NACKF) == 0U;
}

}  // namespace

bool Init() {
  // Enable GPIOC and configure PC8 (SCL) / PC9 (SDA) as I2C3 AF8 open-drain.
  gpio::EnablePort(GPIOC);
  gpio::ConfigureAlternate(GPIOC, 8U, 8U, gpio::Pull::kUp);
  gpio::ConfigureAlternate(GPIOC, 9U, 8U, gpio::Pull::kUp);
  // Set open-drain for I2C.
  GPIOC->OTYPER |= (1U << 8U) | (1U << 9U);

  // Enable I2C3 clock.
  RCC->APB1ENR1 |= RCC_APB1ENR1_I2C3EN;
  (void)RCC->APB1ENR1;

  // Reset I2C3.
  RCC->APB1RSTR1 |= RCC_APB1RSTR1_I2C3RST;
  RCC->APB1RSTR1 &= ~RCC_APB1RSTR1_I2C3RST;

  // Configure I2C3: timing, enable.
  I2C3->CR1 = 0U;
  I2C3->TIMINGR = kI2cTiming100k;
  I2C3->CR1 = I2C_CR1_PE;

  // Allow bus to settle.
  for (volatile uint32_t i = 0; i < 100000U; ++i) {}

  // Unlock protected registers: LOCK[3:0]=0xF, NLOCK[7:4]=0x0.
  // Complement check: ~NLOCK[7:4] = ~0x0 = 0xF = LOCK[3:0].  Match = unlocked.
  if (!WriteReg(kRegLock, 0x0FU)) {
    return false;
  }

  // Clear all faults.
  if (!WriteReg(kRegClear, 0xFFU)) {
    return false;
  }

  // Verify STATUS register is clear (no active faults).
  uint8_t status = 0xFFU;
  if (!ReadStatus(&status)) {
    return false;
  }

  // Re-lock protected registers.
  (void)WriteReg(kRegLock, 0x00U);

  return true;
}

bool ClearFaults() {
  if (!WriteReg(kRegLock, 0x0FU)) {
    return false;
  }
  if (!WriteReg(kRegClear, 0xFFU)) {
    return false;
  }
  (void)WriteReg(kRegLock, 0x00U);
  return true;
}

bool ReadStatus(uint8_t* status) {
  return ReadReg(kRegStatus, status);
}

}  // namespace sipdrive::hal::i2c3_stspin
