#pragma once

#include <stdint.h>

namespace sipdrive::persistent_config {

constexpr uint32_t kCalibTableSize = 128U;

struct PersistentData {
  uint32_t magic;
  uint32_t version;

  // Motor.
  uint32_t motor_pole_pairs;
  float current_limit_a;

  // Position/velocity PID.
  float pos_kp;
  float pos_ki;
  float pos_kd;
  float vel_kp;
  float vel_ki;
  float vel_limit_rad_s;

  // Thermal limits.
  float fault_temp_board_c;
  float fault_temp_stator_c;
  float temp_margin_c;

  // CAN IDs.
  uint32_t can_command_id;
  uint32_t can_telemetry_id;

  // Encoder calibration.
  uint32_t calib_valid;
  float calib_elec_offset_rad;
  uint16_t calib_table[kCalibTableSize];

  // CRC must be last.
  uint32_t crc32;
};

// Load config from flash.  Returns false if magic/CRC mismatch (fills defaults).
bool Load(PersistentData* data);

// Write config to flash (erases page, computes CRC, writes).
bool Save(const PersistentData& data);

// Fill with compile-time defaults.
void LoadDefaults(PersistentData* data);

}  // namespace sipdrive::persistent_config
