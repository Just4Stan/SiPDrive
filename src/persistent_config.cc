#include "persistent_config.h"

#include "config.h"
#include "hal_flash.h"

#include "string_compat.h"

namespace sipdrive::persistent_config {
namespace {

constexpr uint32_t kMagic = 0x4D4D4347U;  // "MMCG"
constexpr uint32_t kVersion = 1U;

// Last 2 KB page of STM32G431VB 128 KB flash.
constexpr uint32_t kFlashPage = 63U;
constexpr uint32_t kFlashAddr = 0x0801F800U;

constexpr uint32_t kCalibValidMagic = 0x43414C42U;  // "CALB"

uint32_t ComputeCrc32(const void* data, uint32_t size) {
  const uint8_t* p = static_cast<const uint8_t*>(data);
  uint32_t crc = 0xFFFFFFFFU;
  for (uint32_t i = 0; i < size; ++i) {
    crc ^= p[i];
    for (uint32_t b = 0; b < 8U; ++b) {
      if ((crc & 1U) != 0U) {
        crc = (crc >> 1U) ^ 0xEDB88320U;
      } else {
        crc >>= 1U;
      }
    }
  }
  return crc ^ 0xFFFFFFFFU;
}

}  // namespace

void LoadDefaults(PersistentData* data) {
  memset(data, 0, sizeof(*data));
  data->magic = kMagic;
  data->version = kVersion;
  data->motor_pole_pairs = config::kMotorPolePairs;
  data->current_limit_a = 10.0f;
  data->pos_kp = 20.0f;
  data->pos_ki = 0.0f;
  data->pos_kd = 1.0f;
  data->vel_kp = 0.5f;
  data->vel_ki = 50.0f;
  data->vel_limit_rad_s = 50.0f;
  data->fault_temp_board_c = 120.0f;
  data->fault_temp_stator_c = 150.0f;
  data->temp_margin_c = 25.0f;
  data->can_command_id = config::kCanCommandId;
  data->can_telemetry_id = config::kCanTelemetryId;
  data->calib_valid = 0U;
  data->calib_elec_offset_rad = 0.0f;
  // calib_table is zeroed by memset.
}

bool Load(PersistentData* data) {
  const PersistentData* flash =
      reinterpret_cast<const PersistentData*>(kFlashAddr);

  if (flash->magic != kMagic || flash->version != kVersion) {
    LoadDefaults(data);
    return false;
  }

  // Verify CRC over everything before the crc32 field.
  const uint32_t payload_size =
      sizeof(PersistentData) - sizeof(uint32_t);
  const uint32_t crc = ComputeCrc32(flash, payload_size);
  if (crc != flash->crc32) {
    LoadDefaults(data);
    return false;
  }

  memcpy(data, flash, sizeof(PersistentData));
  return true;
}

bool Save(const PersistentData& data) {
  // Compute CRC over everything except the crc32 field.
  PersistentData to_write = data;
  to_write.magic = kMagic;
  to_write.version = kVersion;

  const uint32_t payload_size =
      sizeof(PersistentData) - sizeof(uint32_t);
  to_write.crc32 = ComputeCrc32(&to_write, payload_size);

  if (!hal::flash::ErasePage(kFlashPage)) {
    return false;
  }

  return hal::flash::WriteBlock(
      kFlashAddr, &to_write, sizeof(PersistentData));
}

}  // namespace sipdrive::persistent_config
