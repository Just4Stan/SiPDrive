#include <stdint.h>

#include "stm32g431xx.h"

#ifndef SIPDRIVE_USE_PLL170
#define SIPDRIVE_USE_PLL170 1
#endif

uint32_t SystemCoreClock = 16000000U;

void __libc_init_array(void) __attribute__((weak));
void __libc_init_array(void) {}

static void DelayCycles(volatile uint32_t cycles) {
  while (cycles-- != 0U) {
    __NOP();
  }
}

static void SetBusPrescalersDiv1(void) {
  RCC->CFGR &= ~(RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2);
  RCC->CFGR |= RCC_CFGR_HPRE_DIV1 | RCC_CFGR_PPRE1_DIV1 | RCC_CFGR_PPRE2_DIV1;
}

#if !SIPDRIVE_USE_PLL170
static void ClockToHsi16(void) {
  RCC->CR |= RCC_CR_HSION;
  while ((RCC->CR & RCC_CR_HSIRDY) == 0U) {}

  RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_HSI;
  while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSI) {}

  SetBusPrescalersDiv1();

  FLASH->ACR = (FLASH->ACR & ~FLASH_ACR_LATENCY) | FLASH_ACR_LATENCY_4WS;
  SystemCoreClock = 16000000U;
}
#endif

#if SIPDRIVE_USE_PLL170
static void ClockToPll170(void) {
  RCC->CR |= RCC_CR_HSION;
  while ((RCC->CR & RCC_CR_HSIRDY) == 0U) {}

  RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_HSI;
  while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSI) {}

  RCC->CR &= ~RCC_CR_PLLON;
  while ((RCC->CR & RCC_CR_PLLRDY) != 0U) {}

  // 170MHz from HSI16: VCO = 16MHz / 4 * 85 = 340MHz, PLLR = /2.
  // PLLQ = /4 = 85MHz for FDCAN kernel clock.
  RCC->PLLCFGR =
      RCC_PLLCFGR_PLLSRC_HSI |
      (3U << RCC_PLLCFGR_PLLM_Pos) |
      (85U << RCC_PLLCFGR_PLLN_Pos) |
      RCC_PLLCFGR_PLLREN |
      RCC_PLLCFGR_PLLQEN |
      (1U << RCC_PLLCFGR_PLLQ_Pos);

  RCC->APB1ENR1 |= RCC_APB1ENR1_PWREN;
  (void)RCC->APB1ENR1;

  // Range1 boost mode required for highest frequencies.
  PWR->CR5 &= ~PWR_CR5_R1MODE;

  // 4 WS required for 170 MHz in Range 1 Boost mode (RM0440 Table 9).
  FLASH->ACR = (FLASH->ACR & ~FLASH_ACR_LATENCY) | FLASH_ACR_LATENCY_4WS;

  RCC->CR |= RCC_CR_PLLON;
  while ((RCC->CR & RCC_CR_PLLRDY) == 0U) {}

  SetBusPrescalersDiv1();

  RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL;
  while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) {}

  SystemCoreClock = 170000000U;
}
#endif

void SystemCoreClockUpdate(void) {
#if SIPDRIVE_USE_PLL170
  SystemCoreClock = 170000000U;
#else
  SystemCoreClock = 16000000U;
#endif
}

void SystemInit(void) {
#if (__FPU_PRESENT == 1U) && (__FPU_USED == 1U)
  SCB->CPACR |= (3UL << (10U * 2U)) | (3UL << (11U * 2U));
#endif

  SCB->VTOR = FLASH_BASE;

#if SIPDRIVE_USE_PLL170
  ClockToPll170();
#else
  ClockToHsi16();
#endif

  DelayCycles(1024U);
}
