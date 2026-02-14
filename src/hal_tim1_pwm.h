#pragma once

#include <stdint.h>

namespace sipdrive::hal::tim1_pwm {

struct DutyCycles {
  float phase_a;
  float phase_b;
  float phase_c;
};

using UpdateCallback = void (*)();

void Init();
void Start();
void RegisterUpdateCallback(UpdateCallback callback);
void SetDutyCycles(const DutyCycles& duty);
void DisablePowerStage();
void EnablePowerStage();
bool FaultActive();
bool FaultLatched();
void ClearFaultLatch();
uint32_t AutoReload();

}  // namespace sipdrive::hal::tim1_pwm
