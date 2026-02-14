#include "hal_flash.h"

#include "stm32g431xx.h"

namespace sipdrive::hal::flash {
namespace {

constexpr uint32_t kFlashKey1 = 0x45670123U;
constexpr uint32_t kFlashKey2 = 0xCDEF89ABU;
constexpr uint32_t kTimeoutLoops = 1000000U;

bool WaitBsy() {
  for (uint32_t i = 0; i < kTimeoutLoops; ++i) {
    if ((FLASH->SR & FLASH_SR_BSY) == 0U) {
      return true;
    }
  }
  return false;
}

void Unlock() {
  if ((FLASH->CR & FLASH_CR_LOCK) != 0U) {
    FLASH->KEYR = kFlashKey1;
    FLASH->KEYR = kFlashKey2;
  }
}

void Lock() {
  FLASH->CR |= FLASH_CR_LOCK;
}

void ClearErrors() {
  FLASH->SR = FLASH_SR_OPERR | FLASH_SR_PROGERR | FLASH_SR_WRPERR |
              FLASH_SR_PGAERR | FLASH_SR_SIZERR | FLASH_SR_PGSERR |
              FLASH_SR_MISERR | FLASH_SR_FASTERR;
}

}  // namespace

bool ErasePage(uint32_t page_number) {
  if (!WaitBsy()) {
    return false;
  }

  Unlock();
  ClearErrors();

  FLASH->CR = (FLASH->CR & ~(FLASH_CR_PNB | FLASH_CR_MER1)) |
              (page_number << FLASH_CR_PNB_Pos) |
              FLASH_CR_PER;
  FLASH->CR |= FLASH_CR_STRT;

  const bool ok = WaitBsy();

  FLASH->CR &= ~FLASH_CR_PER;
  Lock();
  return ok;
}

bool WriteDoubleWord(uint32_t address, uint64_t data) {
  if ((address & 0x7U) != 0U) {
    return false;
  }
  if (!WaitBsy()) {
    return false;
  }

  Unlock();
  ClearErrors();

  FLASH->CR |= FLASH_CR_PG;

  // STM32G4 flash requires two 32-bit writes for a 64-bit double-word.
  volatile uint32_t* dest = reinterpret_cast<volatile uint32_t*>(address);
  dest[0] = static_cast<uint32_t>(data & 0xFFFFFFFFU);
  dest[1] = static_cast<uint32_t>(data >> 32U);

  const bool ok = WaitBsy();

  FLASH->CR &= ~FLASH_CR_PG;
  Lock();
  return ok;
}

bool WriteBlock(uint32_t address, const void* data, uint32_t size) {
  const uint8_t* src = static_cast<const uint8_t*>(data);
  uint32_t offset = 0U;

  while (offset < size) {
    uint64_t dw = 0xFFFFFFFFFFFFFFFFULL;
    uint32_t chunk = size - offset;
    if (chunk > 8U) {
      chunk = 8U;
    }
    // Copy available bytes into double-word (little-endian), rest stays 0xFF.
    for (uint32_t i = 0; i < chunk; ++i) {
      dw &= ~(static_cast<uint64_t>(0xFFU) << (i * 8U));
      dw |= static_cast<uint64_t>(src[offset + i]) << (i * 8U);
    }

    if (!WriteDoubleWord(address + offset, dw)) {
      return false;
    }
    offset += 8U;
  }
  return true;
}

}  // namespace sipdrive::hal::flash
