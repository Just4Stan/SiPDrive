#pragma once

#include <stdint.h>

#include "persistent_config.h"

namespace sipdrive::hal::qei {

void Init();
void Sample(float dt_s);
int32_t GetPositionCounts();
float GetMechanicalAngleRad();
float GetElectricalAngleRad();
float GetVelocityRadPerSec();
bool IndexSeen();
bool GetPwmMechanicalAngleRad(float* angle_rad);

// Apply calibration data (offset + compensation table) from persistent config.
void SetCalibration(float elec_offset_rad,
                    const uint16_t* table,
                    uint32_t table_size);

}  // namespace sipdrive::hal::qei
