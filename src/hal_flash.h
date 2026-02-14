#pragma once

#include <stdint.h>

namespace sipdrive::hal::flash {

// Erase a single 2 KB page.  page_number is 0-based (G431VB: 0..63).
bool ErasePage(uint32_t page_number);

// Write a 64-bit double-word at the given flash address (must be 8-byte aligned).
bool WriteDoubleWord(uint32_t address, uint64_t data);

// Write an arbitrary block (will be padded to 8-byte alignment with 0xFF).
bool WriteBlock(uint32_t address, const void* data, uint32_t size);

}  // namespace sipdrive::hal::flash
