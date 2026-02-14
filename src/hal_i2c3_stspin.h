#pragma once

#include <stdint.h>

namespace sipdrive::hal::i2c3_stspin {

// Initialise I2C3 (PC8/PC9) and configure the STSPIN32G4 gate driver.
// Returns false on I2C NACK or timeout.
bool Init();

// Clear all STSPIN32G4 faults (write 0xFF to CLEAR register).
bool ClearFaults();

// Read the STATUS register (address 0x80).
bool ReadStatus(uint8_t* status);

}  // namespace sipdrive::hal::i2c3_stspin
