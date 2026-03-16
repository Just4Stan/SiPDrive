#include <stddef.h>
#include <stdint.h>

// Define NAN if not available from standard library
#ifndef NAN
#define NAN __builtin_nanf("")
#endif

#include "calibration.h"
#include "config.h"
#include "foc_core.h"
#include "hal_adc_opamp.h"
#include "hal_fdcan.h"
#include "hal_gpio.h"
#include "hal_i2c3_stspin.h"
#include "hal_qei.h"
#include "hal_tim1_pwm.h"
#include "persistent_config.h"
#include "position_control.h"
#include "thermal.h"
#include "stm32g431xx.h"

namespace {

// Command mode: 0=current, 1=position.
enum class Mode : uint8_t {
  kCurrent = 0U,
  kPosition = 1U,
};

// ISR-visible state.
volatile Mode g_mode = Mode::kCurrent;

// Current-mode command (Id/Iq in amps).
volatile sipdrive::foc::FocCommand g_current_cmd = {0.0f, 0.0f};

// Position-mode command.
volatile sipdrive::position_control::PositionCommand g_position_cmd = {
    NAN, 0.0f, 10.0f, 1.0f, 1.0f};

volatile uint32_t g_control_ticks = 0U;
volatile bool g_heartbeat_due = false;
volatile bool g_calibration_requested = false;

sipdrive::foc::FocCore g_foc;
sipdrive::position_control::PositionState g_pos_state = {};
sipdrive::position_control::PositionConfig g_pos_config = {};
sipdrive::persistent_config::PersistentData g_config = {};

// Latest FOC output for telemetry.
volatile float g_iq_meas_a = 0.0f;
volatile float g_id_meas_a = 0.0f;

#if SIPDRIVE_DEBUG_TIMING
GPIO_TypeDef* const kDebugAdcPort = GPIOC;
constexpr uint8_t kDebugAdcPin = 14U;

void InitDebugTiming() {
  sipdrive::hal::gpio::EnablePort(kDebugAdcPort);
  sipdrive::hal::gpio::ConfigureOutput(kDebugAdcPort, kDebugAdcPin, true);
  sipdrive::hal::gpio::Write(kDebugAdcPort, kDebugAdcPin, false);
}

inline void PulseDebugAdcRead() {
  sipdrive::hal::gpio::Write(kDebugAdcPort, kDebugAdcPin, true);
  sipdrive::hal::gpio::Write(kDebugAdcPort, kDebugAdcPin, false);
}
#else
void InitDebugTiming() {}
inline void PulseDebugAdcRead() {}
#endif

// ---- Byte helpers ----

int16_t ReadInt16Le(const uint8_t* data) {
  return static_cast<int16_t>(
      static_cast<uint16_t>(data[0]) |
      (static_cast<uint16_t>(data[1]) << 8U));
}

uint16_t ReadUint16Le(const uint8_t* data) {
  return static_cast<uint16_t>(data[0]) |
         (static_cast<uint16_t>(data[1]) << 8U);
}

float ReadFloatLe(const uint8_t* data) {
  uint32_t u = static_cast<uint32_t>(data[0]) |
               (static_cast<uint32_t>(data[1]) << 8U) |
               (static_cast<uint32_t>(data[2]) << 16U) |
               (static_cast<uint32_t>(data[3]) << 24U);
  float f;
  __builtin_memcpy(&f, &u, sizeof(f));
  return f;
}

void WriteInt16Le(uint8_t* data, int16_t value) {
  const uint16_t u = static_cast<uint16_t>(value);
  data[0] = static_cast<uint8_t>(u & 0xFFU);
  data[1] = static_cast<uint8_t>((u >> 8U) & 0xFFU);
}

void WriteUint16Le(uint8_t* data, uint16_t value) {
  data[0] = static_cast<uint8_t>(value & 0xFFU);
  data[1] = static_cast<uint8_t>((value >> 8U) & 0xFFU);
}

void WriteFloatLe(uint8_t* data, float value) {
  uint32_t u;
  __builtin_memcpy(&u, &value, sizeof(u));
  data[0] = static_cast<uint8_t>(u & 0xFFU);
  data[1] = static_cast<uint8_t>((u >> 8U) & 0xFFU);
  data[2] = static_cast<uint8_t>((u >> 16U) & 0xFFU);
  data[3] = static_cast<uint8_t>((u >> 24U) & 0xFFU);
}

// ---- CAN protocol ----
// Command frame (ID from config, 16 bytes CAN-FD):
//   [0..3]  position (float, LE) — NaN = disable position loop
//   [4..7]  velocity (float, LE)
//   [8..9]  max_torque_mA (int16, LE)
//   [10..11] kp_scale (uint16, 0-32767 → 0.0-1.0)
//   [12..13] kd_scale (uint16, same)
//   [14]    flags: bit0=clear fault, bit1=start calibration
//   [15]    mode: 0=current, 1=position
//
// Legacy 4-byte current-mode is still accepted (bytes 0-1=Id_mA, 2-3=Iq_mA).

void HandleCommandFrame(const sipdrive::hal::fdcan::CanFrame& frame) {
  if (frame.id != g_config.can_command_id) {
    return;
  }

  if (frame.len >= 16U) {
    // Extended position-mode frame.
    const float pos = ReadFloatLe(&frame.data[0]);
    const float vel = ReadFloatLe(&frame.data[4]);
    const int16_t torque_ma = ReadInt16Le(&frame.data[8]);
    const uint16_t kp_raw = ReadUint16Le(&frame.data[10]);
    const uint16_t kd_raw = ReadUint16Le(&frame.data[12]);
    const uint8_t flags = frame.data[14];
    const uint8_t mode_byte = frame.data[15];

    g_position_cmd.position_rad = pos;
    g_position_cmd.velocity_rad_s = vel;
    g_position_cmd.max_torque_a =
        static_cast<float>(torque_ma) * 0.001f;
    g_position_cmd.kp_scale =
        static_cast<float>(kp_raw) * (1.0f / 32767.0f);
    g_position_cmd.kd_scale =
        static_cast<float>(kd_raw) * (1.0f / 32767.0f);

    g_mode = (mode_byte == 1U) ? Mode::kPosition : Mode::kCurrent;

    // When in current mode via 16-byte frame, use velocity fields as Id/Iq.
    if (g_mode == Mode::kCurrent) {
      g_current_cmd.id_a = 0.0f;
      g_current_cmd.iq_a =
          static_cast<float>(torque_ma) * 0.001f;
    }

    if ((flags & 0x01U) != 0U) {
      sipdrive::hal::tim1_pwm::ClearFaultLatch();
      if (!sipdrive::hal::tim1_pwm::FaultActive()) {
        sipdrive::hal::tim1_pwm::EnablePowerStage();
      }
    }
    if ((flags & 0x02U) != 0U) {
      g_calibration_requested = true;
    }
  } else if (frame.len >= 4U) {
    // Legacy 4-byte current-mode frame.
    const int16_t id_milliamps = ReadInt16Le(&frame.data[0]);
    const int16_t iq_milliamps = ReadInt16Le(&frame.data[2]);

    g_current_cmd.id_a = static_cast<float>(id_milliamps) * 0.001f;
    g_current_cmd.iq_a = static_cast<float>(iq_milliamps) * 0.001f;
    g_mode = Mode::kCurrent;

    if (frame.len >= 5U) {
      const uint8_t flags = frame.data[4];
      if ((flags & 0x01U) != 0U) {
        sipdrive::hal::tim1_pwm::ClearFaultLatch();
        if (!sipdrive::hal::tim1_pwm::FaultActive()) {
          sipdrive::hal::tim1_pwm::EnablePowerStage();
        }
      }
      if ((flags & 0x02U) != 0U) {
        g_calibration_requested = true;
      }
    }
  }
}

// Telemetry frame (24 bytes CAN-FD):
//   [0..3]  position (float, LE)
//   [4..7]  velocity (float, LE)
//   [8..9]  iq_measured_mA (int16, LE)
//   [10..11] id_measured_mA (int16, LE)
//   [12..13] vbus_mV (uint16, LE)
//   [14..15] board_temp (int16, 0.01C units)
//   [16..17] stator_temp (int16, 0.01C units)
//   [18]    fault flags: bit0=hw fault, bit1=can fault, bit2=thermal fault,
//                        bit3=calibrating
//   [19]    mode
//   [20..23] reserved

void SendTelemetryFrame() {
  sipdrive::hal::fdcan::CanFrame frame;
  frame.id = g_config.can_telemetry_id;
  frame.len = 24U;
  frame.fd = true;
  frame.brs = true;
  for (uint32_t i = 0; i < sizeof(frame.data); ++i) {
    frame.data[i] = 0U;
  }

  WriteFloatLe(&frame.data[0],
               sipdrive::hal::qei::GetMechanicalAngleRad());
  WriteFloatLe(&frame.data[4],
               sipdrive::hal::qei::GetVelocityRadPerSec());
  WriteInt16Le(&frame.data[8],
               static_cast<int16_t>(g_iq_meas_a * 1000.0f));
  WriteInt16Le(&frame.data[10],
               static_cast<int16_t>(g_id_meas_a * 1000.0f));

  sipdrive::hal::adc_opamp::SlowAnalogSample slow;
  if (sipdrive::hal::adc_opamp::ReadSlowAnalog(&slow)) {
    WriteUint16Le(&frame.data[12],
                  static_cast<uint16_t>(slow.vbat_v * 1000.0f));
  }

  WriteInt16Le(&frame.data[14],
               static_cast<int16_t>(
                   sipdrive::thermal::GetBoardTempC() * 100.0f));
  WriteInt16Le(&frame.data[16],
               static_cast<int16_t>(
                   sipdrive::thermal::GetStatorTempC() * 100.0f));

  uint8_t faults = 0U;
  if (sipdrive::hal::tim1_pwm::FaultActive()) { faults |= 0x01U; }
  if (sipdrive::hal::fdcan::FaultActive()) { faults |= 0x02U; }
  if (sipdrive::thermal::IsFaulted()) { faults |= 0x04U; }
  frame.data[18] = faults;
  frame.data[19] = static_cast<uint8_t>(g_mode);

  (void)sipdrive::hal::fdcan::Send(frame);
}

// ---- Control loop ISR (40 kHz) ----

void ControlLoopIsr() {
  ++g_control_ticks;

  if (sipdrive::hal::tim1_pwm::FaultActive() ||
      sipdrive::thermal::IsFaulted()) {
    sipdrive::hal::tim1_pwm::DisablePowerStage();
    return;
  }

  sipdrive::hal::qei::Sample(sipdrive::config::kControlPeriodSec);

  // Skip FOC when calibration is driving the motor open-loop from main loop.
  if (sipdrive::calibration::IsActive()) {
    return;
  }

  sipdrive::hal::adc_opamp::CurrentSample sample;
  if (!sipdrive::hal::adc_opamp::ReadInjected(&sample)) {
    return;
  }
  PulseDebugAdcRead();

  // Determine Iq command based on mode.
  float id_cmd = 0.0f;
  float iq_cmd = 0.0f;

  const Mode mode = g_mode;
  if (mode == Mode::kPosition) {
    sipdrive::position_control::PositionCommand pos_cmd;
    pos_cmd.position_rad = g_position_cmd.position_rad;
    pos_cmd.velocity_rad_s = g_position_cmd.velocity_rad_s;
    pos_cmd.max_torque_a = g_position_cmd.max_torque_a;
    pos_cmd.kp_scale = g_position_cmd.kp_scale;
    pos_cmd.kd_scale = g_position_cmd.kd_scale;

    iq_cmd = sipdrive::position_control::Update(
        pos_cmd,
        sipdrive::hal::qei::GetMechanicalAngleRad(),
        sipdrive::hal::qei::GetVelocityRadPerSec(),
        sipdrive::config::kControlPeriodSec,
        g_pos_config,
        &g_pos_state);
  } else {
    id_cmd = g_current_cmd.id_a;
    iq_cmd = g_current_cmd.iq_a;
  }

  // Apply thermal derating.
  const float thermal_scale = sipdrive::thermal::GetCurrentScale();
  id_cmd *= thermal_scale;
  iq_cmd *= thermal_scale;

  sipdrive::foc::FocInputs inputs;
  inputs.ia_a = sample.ia_a;
  inputs.ib_a = sample.ib_a;
  inputs.electrical_angle_rad =
      sipdrive::hal::qei::GetElectricalAngleRad();
  inputs.dt_s = sipdrive::config::kControlPeriodSec;

  sipdrive::foc::FocCommand command;
  command.id_a = id_cmd;
  command.iq_a = iq_cmd;

  const sipdrive::foc::FocOutput output = g_foc.Step(inputs, command);
  sipdrive::hal::tim1_pwm::SetDutyCycles(output.duty);

  g_iq_meas_a = output.iq_meas_a;
  g_id_meas_a = output.id_meas_a;

  if ((g_control_ticks % sipdrive::config::kHeartbeatTicks) == 0U) {
    g_heartbeat_due = true;
  }
}

void ApplyConfigFromFlash() {
  // Position/velocity PID.
  g_pos_config.pos_kp = g_config.pos_kp;
  g_pos_config.vel_kp = g_config.vel_kp;
  g_pos_config.vel_ki = g_config.vel_ki;
  g_pos_config.vel_ilimit = g_config.current_limit_a;
  g_pos_config.vel_limit_rad_s = g_config.vel_limit_rad_s;

  // Thermal limits.
  sipdrive::thermal::ThermalConfig tc;
  tc.board_fault_temp_c = g_config.fault_temp_board_c;
  tc.stator_fault_temp_c = g_config.fault_temp_stator_c;
  tc.warning_margin_c = g_config.temp_margin_c;
  sipdrive::thermal::Init(tc);

  // Encoder calibration.
  if (g_config.calib_valid == 0x43414C42U) {
    sipdrive::hal::qei::SetCalibration(
        g_config.calib_elec_offset_rad,
        g_config.calib_table,
        sipdrive::persistent_config::kCalibTableSize);
  }
}

}  // namespace

extern "C" int main(void) {
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

  InitDebugTiming();

  // Load persistent config (or defaults if flash is empty/corrupt).
  sipdrive::persistent_config::Load(&g_config);

  // Initialize STSPIN32G4 gate driver via I2C3.
  (void)sipdrive::hal::i2c3_stspin::Init();

  sipdrive::hal::tim1_pwm::Init();
  sipdrive::hal::tim1_pwm::RegisterUpdateCallback(ControlLoopIsr);

  sipdrive::hal::adc_opamp::Init();
  sipdrive::hal::adc_opamp::CalibrateOffsets(
      sipdrive::config::kCurrentOffsetSamples);

  sipdrive::hal::qei::Init();
  sipdrive::hal::fdcan::Init();

  ApplyConfigFromFlash();

  g_foc.Reset();
  sipdrive::position_control::Reset(&g_pos_state);

  sipdrive::hal::tim1_pwm::Start();
  sipdrive::hal::tim1_pwm::EnablePowerStage();

  while (true) {
    const uint32_t control_ticks = g_control_ticks;
    sipdrive::hal::adc_opamp::ServiceRegular(control_ticks);

    // Update thermal protection.
    sipdrive::hal::adc_opamp::SlowAnalogSample slow;
    if (sipdrive::hal::adc_opamp::ReadSlowAnalog(&slow)) {
      sipdrive::thermal::Update(slow.board_ntc_v, slow.stator_ntc_v);
      if (sipdrive::thermal::IsFaulted()) {
        sipdrive::hal::tim1_pwm::DisablePowerStage();
      }
    }

    // Handle incoming CAN frames.
    sipdrive::hal::fdcan::CanFrame frame;
    if (sipdrive::hal::fdcan::Receive(&frame)) {
      HandleCommandFrame(frame);
    }

    // Calibration state machine (runs in main loop, ~1 kHz effective).
    if (g_calibration_requested) {
      g_calibration_requested = false;
      sipdrive::calibration::Start();
    }

    {
      float da = 0.5f, db = 0.5f, dc = 0.5f;
      const auto calib_state =
          sipdrive::calibration::Update(&da, &db, &dc);

      if (calib_state == sipdrive::calibration::State::kForward ||
          calib_state == sipdrive::calibration::State::kReverse) {
        // During calibration, bypass FOC and directly set duty cycles.
        sipdrive::hal::tim1_pwm::DutyCycles duty;
        duty.phase_a = da;
        duty.phase_b = db;
        duty.phase_c = dc;
        sipdrive::hal::tim1_pwm::SetDutyCycles(duty);
      } else if (calib_state == sipdrive::calibration::State::kDone) {
        // Save calibration result to flash.
        const auto& result = sipdrive::calibration::GetResult();
        if (result.valid) {
          g_config.calib_valid = 0x43414C42U;
          g_config.calib_elec_offset_rad = result.electrical_offset_rad;
          for (uint32_t i = 0;
               i < sipdrive::persistent_config::kCalibTableSize; ++i) {
            g_config.calib_table[i] = result.compensation_table[i];
          }
          sipdrive::persistent_config::Save(g_config);

          // Apply immediately.
          sipdrive::hal::qei::SetCalibration(
              g_config.calib_elec_offset_rad,
              g_config.calib_table,
              sipdrive::persistent_config::kCalibTableSize);
        }
      }
    }

    if (g_heartbeat_due) {
      g_heartbeat_due = false;
      SendTelemetryFrame();
    }

    sipdrive::hal::fdcan::Service();
    __WFI();
  }
}
