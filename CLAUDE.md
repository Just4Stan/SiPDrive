# CLAUDE.md

This file provides guidance to Claude Code when working with the SiPDrive FOC controller firmware.

## Project Overview

**SiPDrive** is a compact Field-Oriented Control (FOC) controller for Quasi-Direct Drive (QDD) actuators. It combines an STSPIN32G4 System-in-Package (SiP), MT6701 angular encoder, and CAN-FD communication to provide torque, position, speed, and angle control with backdrivable operation.

### Project Goals
- Compact, integrated motor controller with on-board sensing and power stage
- High-frequency (40kHz) FOC control loop for smooth torque control
- CAN-FD communication for command/telemetry
- Encoder calibration with compensation table for accuracy
- Thermal protection and fault handling
- Persistent configuration storage in flash

## Build Status & Current State

**Last Build**: 2026-02-14
**Status**: ✅ **COMPILES SUCCESSFULLY** | ⚠️ **UNTESTED ON HARDWARE**

### What Works
- ✅ **Firmware compiles** - CMake build system functional
- ✅ **Binary generation** - ELF, BIN, HEX formats available
- ✅ **Code complete** - ~3,100 LOC with full FOC implementation
- ✅ **Pin assignments validated** - Verified against KiCad schematic
- ✅ **Dependencies resolved** - CMSIS Core + STM32G4 headers integrated

### Memory Usage (Current)

```
Memory Region    Used Size    Total Size    Usage      Status
────────────────────────────────────────────────────────────────
FLASH            11,056 B     128 KB        8.44%      ✅ Excellent
RAM              13,344 B      22 KB       59.23%      ✅ Healthy
CCM RAM           8,192 B      10 KB       80.00%      ✅ In Use
```

**RAM Usage Notes**:
- A large calibration buffer was moved into CCM RAM.
- Main SRAM headroom is now significantly better.
- Stack is still fixed at 4KB and not yet profiled on hardware.

**💡 Optimization Opportunities**:
1. **Stack profiling + sizing**: validate real worst-case stack and reduce `_Min_Stack_Size` if safe.
2. **Move additional hot data/code to CCM** where deterministic access is beneficial.
3. **Audit remaining `.bss`** and promote static constants to flash where appropriate.

### Build Artifacts

Located in `build/`:
- **SiPDrive.elf** (42KB) - ELF executable with debug symbols for GDB
- **SiPDrive.bin** (11KB) - Raw binary for flashing via `st-flash`
- **SiPDrive.hex** (31KB) - Intel HEX format for STM32CubeProgrammer
- **SiPDrive.map** (54KB) - Detailed memory map showing all symbols

### Build System Setup (macOS-Specific)

**Toolchain**:
- ARM GCC 15.2.0 (Homebrew: `brew install arm-none-eabi-gcc`)
- CMake 4.2.3 (Homebrew: `brew install cmake`)
- Cortex-M4F hard FPU configuration

**Dependencies** (auto-downloaded to `external/`):
- ARM CMSIS Core 5.9.0 (~7K files)
- STM32G4 CMSIS Device headers

**Custom Runtime Library** ([runtime_support.cc](src/runtime_support.cc)):
- Provides math functions: `sinf`, `cosf`, `sqrtf`, `fmodf`, `atan2f`, `fabsf`
- Provides string functions: `memset`, `memcpy`, `memmove`, `memcmp`
- **Why needed**: Homebrew ARM toolchain lacks newlib (standard C library)
- **Quality**: Basic Taylor series implementations - may need improvement for production

### Known Build Issues

1. **FDCAN RXESC/TXESC registers not defined** in STM32G431 CMSIS headers
   - **Workaround**: Lines commented out in [hal_fdcan.cc](src/hal_fdcan.cc:136-137)
   - **Impact**: CAN-FD >8 byte payload behavior must be verified on target
   - **TODO**: Add explicit register writes or header-safe fallback to guarantee FD payload sizing

2. **Math functions use approximations**
   - `sinf`/`cosf`: Taylor series (5 terms) - accuracy ~0.001 for ±π
   - **Impact**: May affect FOC accuracy at high speeds or with precise encoders
   - **TODO**: Profile accuracy, consider using CORDIC for all trig (not just Park transform)

3. **No unit tests or simulation**
   - Code has never been executed
   - FOC transforms validated only by inspection
   - **TODO**: Create host-side tests for FOC math, configuration parsing

### Flashing Instructions

**Using ST-LINK** (recommended):
```bash
st-flash write build/SiPDrive.bin 0x08000000
```

**Using OpenOCD**:
```bash
openocd -f interface/stlink.cfg -f target/stm32g4x.cfg \
  -c "program build/SiPDrive.elf verify reset exit"
```

**Using STM32CubeProgrammer**:
1. Open STM32CubeProgrammer GUI
2. Connect via ST-LINK (SWD)
3. Load `build/SiPDrive.hex` or `build/SiPDrive.bin`
4. Flash and verify

### Pre-Flight Checklist

Before flashing to hardware:
- [ ] Verify motor pole pairs match `config.h` (`kMotorPolePairs = 7`)
- [ ] Check encoder CPR is correct (`kEncoderCountsPerRev = 16384`)
- [ ] Confirm current sense shunt value (`kShuntOhm = 0.005`)
- [ ] Verify OPAMP gain matches hardware (`kOpampGain = 16.0`)
- [ ] Set appropriate current limits (`config.h` or persistent config)
- [ ] Ensure bus voltage is safe (`kBusVoltageV = 24.0f`)
- [ ] Review thermal limits (default: board 120°C, stator 150°C, margin 25°C)

**⚠️ CRITICAL SAFETY WARNINGS**:
1. **Code is UNTESTED** - Expect bugs, unpredictable behavior
2. **Current sensing polarity unknown** - Wrong polarity → motor runaway
3. **Electrical angle offset unknown** - Will need calibration before smooth operation
4. **No hardware current limit** - Software only, may fail
5. **Stack unprofiled on target** - overflow risk still exists until measured

## Hardware Architecture

### Core Components

1. **STSPIN32G4 (U1)**
   - System-in-Package containing:
     - STM32G431CBU6 microcontroller (Cortex-M4F @ 170MHz)
     - Integrated gate driver (3-phase half-bridge)
     - Buck converter for MCU power
     - 2x operational amplifiers for current sensing
   - **Memory Constraints**: 128KB Flash, 32KB SRAM (22KB general + 10KB CCM)
   - Datasheet: `hw/SiPDrive/datasheets/stspin32g4.pdf`

2. **MT6701 Magnetic Encoder (U2)**
   - 14-bit magnetic angle sensor (16384 CPR in software config)
   - SSI/ABZ/UVW output modes
   - I2C configuration interface
   - Datasheet: `hw/SiPDrive/datasheets/MT6701.pdf`

3. **TCAN1057A CAN Transceiver (U3)**
   - CAN-FD compatible transceiver
   - Up to 5 Mbps data rate
   - Datasheet: `hw/SiPDrive/datasheets/tcan1057a-q1.pdf`

4. **STM32G431 MCU Details**
   - Reference Manual: `hw/SiPDrive/datasheets/rm0440-stm32g4-series-advanced-armbased-32bit-mcus-stmicroelectronics.pdf`
   - Datasheet: `hw/SiPDrive/datasheets/stm32g431c6.pdf`

### Hardware Limitations

⚠️ **Critical Constraints**:
- **Only 2 ADC channels available** for current sensing (OPAMP1→ADC1_IN13, OPAMP2→ADC2_IN16)
  - Measures only Ia and Ib (Ic calculated from Ia + Ib + Ic = 0)
- **128KB Flash** - code size must be minimized (currently ~3000 LOC)
- **Deadtime compensation not implemented** (TIM1 hardware deadtime insertion is configured via BDTR)
- **No hardware current limit** - all protection is software-based
- **MT6701 MODE pin currently unconnected in netlist** (`unconnected-(U2-MODE-Pad14)`)
  - Current firmware assumes ABZ/PWM usage; hardware strap/config for MODE is still a board action item

### Pin Assignments (Validated 2026-02-09, naming/paths rechecked 2026-02-14)

Key peripherals and their pin assignments:
- **TIM1**: 3-phase PWM generation (complementary outputs for gate driver)
  - CH1/CH1N, CH2/CH2N, CH3/CH3N
- **TIM3**: PWM capture for MT6701 absolute angle (PB5 → TIM3_CH2)
- **TIM4**: QEI (quadrature encoder interface) for MT6701 ABZ (PB7=A, PB6=B, PB4=Z)
- **ADC1/ADC2**: OPAMP outputs for phase current sensing (injected conversions)
  - OPAMP1 → ADC1_IN13, OPAMP2 → ADC2_IN16
- **ADC1/ADC2**: Regular conversions for sensors
  - PC0 (ADC_IN6) = VBAT_SENSE
  - PC1 (ADC_IN7) = BOARD_NTC
  - PC2 (ADC_IN8) = STATOR_NTC
- **FDCAN1**: CAN-FD communication (PA11=RX, PA12=TX, PA10=TCAN1057A S mode-select)
- **I2C3**: STSPIN32G4 gate driver configuration
- **CORDIC**: Hardware sin/cos accelerator (used in FOC transforms)

## Firmware Architecture

### Directory Structure

```
SiPDrive/
├── CMakeLists.txt           # Build configuration
├── link/
│   └── stm32g431vb.ld       # Linker script (memory layout)
├── src/
│   ├── main.cc              # Main loop, CAN protocol, control loop ISR
│   ├── config.h             # Compile-time configuration constants
│   │
│   ├── foc_core.{h,cc}      # FOC algorithm (Clarke/Park transforms, PI current control)
│   ├── position_control.{h,cc}  # Position/velocity PID controller
│   ├── calibration.{h,cc}   # Encoder calibration state machine
│   │
│   ├── hal_tim1_pwm.{h,cc}  # TIM1 PWM + control loop timer (40kHz ISR)
│   ├── hal_adc_opamp.{h,cc} # ADC + OPAMP config (injected + regular conversions)
│   ├── hal_qei.{h,cc}       # Quadrature encoder interface (TIM3/4)
│   ├── hal_fdcan.{h,cc}     # CAN-FD protocol handler
│   ├── hal_i2c3_stspin.{h,cc}  # STSPIN32 gate driver I2C config
│   ├── hal_flash.{h,cc}     # Flash read/write for persistent config
│   ├── hal_gpio.{h,cc}      # GPIO utilities
│   │
│   ├── thermal.{h,cc}       # Thermal protection (NTC monitoring + derating)
│   ├── persistent_config.{h,cc}  # Flash-based config storage (CRC protected)
│   │
│   ├── startup_stm32g431xx.s  # Assembly startup code + vector table
│   └── system_stm32g4xx.c   # System clock configuration
│
├── hw/SiPDrive/
│   ├── datasheets/          # Component datasheets
│   ├── kicad/               # KiCad symbol/footprint libraries
│   ├── SiPDrive.kicad_*   # KiCad project files
│   └── SiPDrive.net       # Netlist (shows hardware connections)
│
├── external/                # CMSIS dependencies (CMSIS Core + STM32G4 device)
└── cmake/                   # CMake helper scripts
```

### Control Flow

#### Startup Sequence (`main.cc`)
1. Enable DWT cycle counter for profiling
2. Load persistent config from flash (or defaults if invalid)
3. Initialize STSPIN32G4 gate driver via I2C3
4. Initialize TIM1 PWM (40kHz) + register ISR callback
5. Initialize ADC + OPAMP, calibrate current offsets (1024 samples)
6. Initialize QEI (encoder interface)
7. Initialize FDCAN (CAN-FD)
8. Apply config (PID gains, thermal limits, encoder calibration)
9. Enable TIM1 and power stage
10. Enter main loop

#### Main Loop (low priority, ~1kHz effective)
- Service regular ADC conversions (Vbus, NTC temperatures)
- Update thermal protection state
- Handle CAN-FD RX messages (command frames)
- Run calibration state machine (if active)
- Send telemetry frames (heartbeat, 10Hz)
- Sleep via `__WFI()` until next event

#### Control Loop ISR (40kHz, highest priority)
Triggered by TIM1 Update event:
1. Increment tick counter
2. Check fault conditions (hardware fault, thermal fault)
3. Sample encoder position/velocity
4. Read injected ADC values (Ia, Ib)
5. Determine Id/Iq command based on mode:
   - **Current mode**: Use commanded Id/Iq directly
   - **Position mode**: Run position PID → velocity PID → Iq command
6. Apply thermal derating scale
7. Run FOC algorithm:
   - Clarke transform (abc → αβ)
   - Park transform (αβ → dq) using CORDIC sin/cos
   - PI current controllers (Id, Iq)
   - Inverse Park (dq → αβ)
   - Inverse Clarke + SVPWM (αβ → abc duty cycles)
8. Update TIM1 CCR registers (duty cycles)
9. Queue telemetry update if heartbeat due

### FOC Algorithm Details

**Field-Oriented Control Implementation** ([foc_core.cc](src/foc_core.cc)):
- **Clarke Transform**: 2-phase current measurement (Ia, Ib) → α-β frame
  - `i_alpha = ia`
  - `i_beta = (ia + 2*ib) / √3`
- **Park Transform**: α-β → d-q rotating frame using electrical angle
  - Uses STM32G4 **CORDIC hardware accelerator** (Q1.31 format, 5 iterations)
  - `id = cos(θ)*i_alpha + sin(θ)*i_beta`
  - `iq = -sin(θ)*i_alpha + cos(θ)*i_beta`
- **PI Current Controllers**: Independent Id/Iq control
  - Default gains: Kp=0.30, Ki=200.0 (configured in `config.h`)
  - Integrator anti-windup clamped to ±Vbus/2
- **Inverse Transforms**: d-q → α-β → a-b-c
- **SVPWM**: Space Vector PWM via common-mode voltage centering
  - Duty range: 2%-98% (configured in `config.h`)
  - Clamping ensures linear modulation region

### Control Modes

1. **Current Mode** (`Mode::kCurrent`)
   - Direct Id/Iq control
   - Used for: torque/current control during bring-up and runtime
   - Command: 4-byte CAN frame (Id_mA, Iq_mA)

2. **Position Mode** (`Mode::kPosition`)
   - Cascaded PID: Position → Velocity → Torque (Iq)
   - Position error → velocity command → Iq command
   - Gains scaled by kp_scale, kd_scale (0.0-1.0)
   - Command: 16-byte CAN-FD frame (pos, vel, max_torque, kp_scale, kd_scale)

### CAN-FD Protocol

**Command Frame** (ID from config, default 0x101):
- **4-byte legacy mode** (current control):
  - [0-1]: Id (int16, milliamps, LE)
  - [2-3]: Iq (int16, milliamps, LE)
  - [4]: Flags (optional)
    - Bit 0: Clear fault
    - Bit 1: Start calibration

- **16-byte extended mode** (position/current):
  - [0-3]: Position (float32, radians, LE) - NaN disables position loop
  - [4-7]: Velocity (float32, rad/s, LE)
  - [8-9]: Max torque (int16, milliamps, LE)
  - [10-11]: kp_scale (uint16, 0-32767 → 0.0-1.0)
  - [12-13]: kd_scale (uint16, 0-32767 → 0.0-1.0)
  - [14]: Flags (same as above)
  - [15]: Mode (0=current, 1=position)

**Telemetry Frame** (ID from config, default 0x181, 24 bytes, 10Hz):
- [0-3]: Position (float32, radians, LE)
- [4-7]: Velocity (float32, rad/s, LE)
- [8-9]: Iq measured (int16, milliamps, LE)
- [10-11]: Id measured (int16, milliamps, LE)
- [12-13]: Vbus (uint16, millivolts, LE)
- [14-15]: Board temperature (int16, 0.01°C, LE)
- [16-17]: Stator temperature (int16, 0.01°C, LE)
- [18]: Fault flags
  - Bit 0: Gate-driver fault input (PE15 nFAULT / optional BKIN path)
  - Bit 1: CAN fault
  - Bit 2: Thermal fault
  - Bit 3: Reserved for calibrating flag (not currently set in firmware)
- [19]: Current mode
- [20-23]: Reserved

### Configuration System

**Persistent Configuration** ([persistent_config.h](src/persistent_config.h)):
- Stored in flash (last page)
- CRC32 protected
- Magic number: 0x4D4D4347 ('MMCG')
- Fields:
  - Motor: pole pairs, current limit
  - PID gains: position Kp/Ki/Kd, velocity Kp/Ki, velocity limit
  - Thermal: board/stator fault temps, warning margin
  - CAN IDs: command/telemetry
  - Encoder calibration: electrical offset, 128-entry compensation table

**Compile-Time Configuration** ([config.h](src/config.h)):
- CMake variables passed via `-D` flags
- Key parameters:
  - `SIPDRIVE_SYS_CLOCK_HZ`: 170 MHz (PLL) or 16 MHz (HSI)
  - `SIPDRIVE_CONTROL_LOOP_HZ`: 40 kHz (default)
  - `SIPDRIVE_PWM_FREQUENCY_HZ`: 40 kHz
  - `SIPDRIVE_TIM1_DEADTIME_NS`: 250ns
  - `SIPDRIVE_ENCODER_CPR`: 16384 (MT6701 14-bit)
  - `SIPDRIVE_ADC_IA_INJECTED_CHANNEL`: 13 (OPAMP1→ADC1_IN13)
  - `SIPDRIVE_ADC_IB_INJECTED_CHANNEL`: 16 (OPAMP2→ADC2_IN16)

### Encoder Calibration

**Purpose**: Compensate for encoder non-linearity and determine electrical angle offset.

**Calibration Process** ([calibration.cc](src/calibration.cc)):
1. **Forward Sweep**: Drive motor with fixed d-axis voltage, sweep electrical angle 0→2π
   - Sample encoder position at N uniformly spaced electrical angles
   - Record encoder error vs. expected mechanical angle
2. **Reverse Sweep**: Repeat in opposite direction to average out cogging torque
3. **Compute Compensation Table**: 128-entry lookup table (encoder_angle → correction)
4. **Save to Flash**: Store electrical offset + compensation table in persistent config

**Usage**: After calibration, QEI module applies compensation table to raw encoder readings.

### Thermal Protection

**Two-Level Protection** ([thermal.cc](src/thermal.cc)):
1. **Warning Level**: Linearly derate current limit when temp exceeds (fault_temp - margin)
   - `current_scale = 1.0 → 0.0` over warning range
2. **Fault Level**: Disable power stage when temp exceeds fault threshold
   - Board NTC: Default 120°C fault, 25°C margin
   - Stator NTC: Default 150°C fault, 25°C margin

**Temperature Sensing**:
- NTC thermistors read via ADC regular channels
- Piecewise lookup-table interpolation (voltage→temperature)
- Sampled at 1kHz in main loop

## Build System

### CMake Build Process

**Prerequisites**:
- ARM GNU Toolchain (arm-none-eabi-gcc)
- CMSIS Core headers (Cortex-M4)
- STM32G4 CMSIS Device headers
- CMake ≥ 3.20

**External Dependencies** (must be populated in `external/`):
1. **CMSIS Core** (`external/CMSIS/Core/Include/`):
   - `core_cm4.h`, `cmsis_gcc.h`, etc.
   - Download from: https://github.com/ARM-software/CMSIS_5
2. **STM32G4 CMSIS Device** (`external/cmsis_device_g4/`):
   - `Include/stm32g431xx.h`, `Include/system_stm32g4xx.h`
   - Download from: https://github.com/STMicroelectronics/cmsis_device_g4

**Build Commands**:
```bash
# Configure (from repository root)
cmake -B build -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi-gcc.cmake \
  -DSIPDRIVE_CMSIS_CORE_DIR=external/CMSIS/Core/Include \
  -DSIPDRIVE_STM32G4_DEVICE_DIR=external/cmsis_device_g4

# Build
cmake --build build

# Output: build/SiPDrive.elf (with .map file)
```

**CMake Options**:
- `SIPDRIVE_USE_PLL170`: Enable 170MHz PLL (default ON)
- `SIPDRIVE_USE_BKIN`: Enable TIM1 break input on PE15 (default OFF)
- `SIPDRIVE_DEBUG_TIMING`: GPIO timing markers for ISR profiling (default OFF)
- `SIPDRIVE_CONTROL_LOOP_HZ`: Control loop frequency (default 40000)
- `SIPDRIVE_PWM_FREQUENCY_HZ`: PWM frequency (default 40000)

### Memory Layout

**Linker Script** ([link/stm32g431vb.ld](link/stm32g431vb.ld)):
```
FLASH:  0x08000000, 128KB  (code + const data)
RAM:    0x20000000, 22KB   (general SRAM)
CCMRAM: 0x10000000, 10KB   (core-coupled memory)
```

**Current Memory Usage** (~3,100 LOC, build 2026-02-14):
- **Flash**: 11,056 bytes / 128KB (8.44%) - ✅ Excellent
- **RAM**: 13,344 bytes / 22KB (59.23%) - ✅ Healthy
- **CCM RAM**: 8,192 bytes / 10KB (80.00%) - ✅ In active use

**Detailed Section Breakdown**:
```
.text        10,800 bytes  - Code (75.8% of used Flash)
.data            56 bytes  - Initialized variables
RAM .bss      9,260 bytes  - Main SRAM uninitialized data
.ccm_bss      8,192 bytes  - Calibration buffer moved to CCM SRAM
._user_heap_stack  4,100 bytes  - Stack + heap
```

⚠️ **Remaining risk**: stack size is fixed and unprofiled on hardware. Next optimization should be stack watermarking/measurement.

## Development Workflow

### Typical STM32 Development Cycle

1. **Write/Modify Code**
   - Edit source files in `src/`
   - Update `config.h` or `CMakeLists.txt` for configuration changes

2. **Build Firmware**
   ```bash
   cmake --build build
   ```

3. **Flash to Target**
   - Using ST-LINK: `st-flash write build/SiPDrive.bin 0x08000000`
   - Using OpenOCD: `openocd -f interface/stlink.cfg -f target/stm32g4x.cfg -c "program build/SiPDrive.elf verify reset exit"`
   - Using STM32CubeProgrammer GUI

4. **Debug**
   - GDB + OpenOCD: `arm-none-eabi-gdb build/SiPDrive.elf`
   - ST-LINK GDB server
   - SWD/JTAG interface on target board

5. **Test**
   - Send CAN commands using Linux `can-utils` or Python `python-can`
   - Monitor telemetry frames
   - Use oscilloscope to verify PWM signals, current waveforms

### PoC Bring-Up Workflow (Flash → Motor Motion)

For this firmware, the control interface is currently **CAN-FD only**.

1. **Bench setup first (no load)**
   - Power stage at safe voltage/current-limited supply.
   - Motor mechanically free.
   - ST-LINK connected for recovery.
   - CAN-FD adapter connected with proper 120Ω termination.

2. **Flash firmware**
   ```bash
   st-flash write build/SiPDrive.bin 0x08000000
   ```

3. **Bring up host CAN-FD interface**
   ```bash
   sudo ip link set can0 down
   sudo ip link set can0 type can bitrate 1000000 dbitrate 5000000 fd on
   sudo ip link set can0 up
   ```

4. **Verify telemetry is alive (default ID `0x181`)**
   ```bash
   candump can0,181:7FF
   ```

5. **Send low-risk current command first**
   - Legacy 4-byte frame: `[Id_mA, Iq_mA]` little-endian.
   - Example: `Id=0 mA`, `Iq=+1000 mA` on command ID `0x101`:
   ```bash
   cansend can0 101##10000E803
   ```

6. **Move to position mode only after current control is stable**
   - Use 16-byte command frame (`main.cc` protocol).
   - Start with small torque limits and low gains.

7. **Run calibration**
   - Set command flag bit1 (`start calibration`) in command frame.
   - Confirm calibration result is saved and reloaded from flash.

8. **Validate fundamentals with scope/logs**
   - PWM centered and complementary outputs valid.
   - Current signs/polarity correct.
   - Encoder angle/velocity direction coherent.
   - Thermal and fault flags behave as expected.

### Code Style Guidelines

- **C++ Standard**: C++17 (configured in CMakeLists.txt)
- **Naming**:
  - Classes: `PascalCase` (e.g., `FocCore`)
  - Functions: `PascalCase` (e.g., `GetVelocityRadPerSec`)
  - Variables: `snake_case` (e.g., `electrical_angle_rad`)
  - Constants: `kPascalCase` (e.g., `kControlLoopHz`)
  - Members: `snake_case_` with trailing underscore (e.g., `id_integrator_`)
- **Namespaces**: `sipdrive::module` (e.g., `sipdrive::foc`, `sipdrive::hal::tim1_pwm`)
- **Headers**: `#pragma once` (no include guards)
- **Optimization**: ISR code should be inlined and optimized for speed; use `-Os` globally but consider `__attribute__((hot))` for ISR paths

### HAL Abstraction Layer

All hardware access is abstracted through `hal_*` modules:
- **hal_tim1_pwm**: TIM1 PWM generation + control loop ISR
- **hal_adc_opamp**: ADC + OPAMP configuration and sampling
- **hal_qei**: Quadrature encoder interface (TIM3/4)
- **hal_fdcan**: CAN-FD peripheral driver
- **hal_i2c3_stspin**: I2C3 for STSPIN32 gate driver config
- **hal_flash**: Flash read/write for persistent config
- **hal_gpio**: GPIO utilities

⚠️ **When modifying HAL**: Ensure ISR timing is not impacted. Use `SIPDRIVE_DEBUG_TIMING` to profile critical paths.

## Hardware Design (KiCad)

### KiCad Project Files
- **Schematic**: `hw/SiPDrive/SiPDrive.kicad_sch`
- **PCB Layout**: `hw/SiPDrive/SiPDrive.kicad_pcb`
- **Netlist**: `hw/SiPDrive/SiPDrive.net` (useful for tracing connections)
- **Custom Libraries**: `hw/SiPDrive/kicad/libs/` (symbols + footprints)

### Key Design Considerations
- **Compact Form Factor**: Controller mounts on motor back
- **Thermal Management**: NTC thermistors for board + stator temperature
- **Power**: Buck converter in STSPIN32 provides 3.3V MCU supply
- **Current Sensing**: OPAMP gain = 16x, shunt = 5mΩ (configured in `config.h`)
- **Encoder Interface**: MT6701 ABZ plus optional PWM absolute capture (single-ended)

## Common Tasks

### Adding a New Configuration Parameter

1. Add CMake option in `CMakeLists.txt`:
   ```cmake
   set(SIPDRIVE_NEW_PARAM "default_value" CACHE STRING "Description")
   ```

2. Add compile definition:
   ```cmake
   target_compile_definitions(SiPDrive.elf PRIVATE
     SIPDRIVE_NEW_PARAM=${SIPDRIVE_NEW_PARAM}
   )
   ```

3. Add fallback in `config.h`:
   ```cpp
   #ifndef SIPDRIVE_NEW_PARAM
   #define SIPDRIVE_NEW_PARAM default_value
   #endif
   constexpr auto kNewParam = static_cast<type>(SIPDRIVE_NEW_PARAM);
   ```

### Modifying FOC Gains

**Compile-Time** (in `config.h`):
```cpp
constexpr float kIdKp = 0.30f;
constexpr float kIdKi = 200.0f;
constexpr float kIqKp = 0.30f;
constexpr float kIqKi = 200.0f;
```

**Runtime** (via persistent config):
- Modify `PersistentData` in flash (requires custom CAN command or debug tool)
- Gains currently hardcoded in config.h; consider moving to `PersistentData` for runtime tuning

### Adding a New CAN Command

1. Define command structure in `main.cc` `HandleCommandFrame()`
2. Parse CAN frame data using byte helpers (`ReadFloatLe`, etc.)
3. Update corresponding global state (e.g., `g_current_cmd`, `g_position_cmd`)
4. Document protocol in this file and/or host-side library

### Implementing a New Fault Condition

1. Add fault flag bit in telemetry frame definition (`main.cc`)
2. Implement fault detection in main loop or ISR
3. Set fault state variable (e.g., `volatile bool g_custom_fault`)
4. In `ControlLoopIsr()`, check fault and disable power stage:
   ```cpp
   if (g_custom_fault) {
     sipdrive::hal::tim1_pwm::DisablePowerStage();
     return;
   }
   ```
5. Provide fault clear mechanism via CAN command flag

## Debugging and Troubleshooting

### Common Issues

1. **No Current Sensing**
   - Check OPAMP configuration (`hal_adc_opamp.cc`)
   - Verify ADC injected channel mapping matches OPAMP outputs
   - Use oscilloscope to verify OPAMP outputs are within 0-3.3V

2. **Motor Not Spinning**
   - Check TIM1 PWM outputs with oscilloscope
   - Verify gate driver is enabled (`hal_i2c3_stspin.cc`)
   - Check for fault flags in telemetry
   - Verify encoder is reading valid position

3. **Encoder Calibration Fails**
   - Ensure motor is free to rotate (no load)
   - Check encoder electrical connection (ABZ signals)
   - Verify QEI configuration (`hal_qei.cc`)

4. **CAN Communication Issues**
   - Check CAN transceiver connections
   - Verify bus termination (120Ω resistors)
   - Ensure bitrate matches host controller (nominal 1 Mbps, data 5 Mbps, FD+BRS)
   - Use `candump` or oscilloscope to debug

5. **Flash is Full**
   - Use `arm-none-eabi-size` to analyze sections
   - Consider moving const data to flash instead of RAM
   - Remove unused features (e.g., debug timing markers)
   - Use `-Os` or `-Oz` optimization

### Debug Tools

- **CMake Option**: `SIPDRIVE_DEBUG_TIMING=ON` enables GPIO toggling for ISR profiling
  - Pulse on PC14 indicates ADC read timing
  - Use logic analyzer or oscilloscope to measure ISR duration

- **SWD Debugging**: Use ST-LINK + GDB to set breakpoints, inspect variables
  - Note: Breakpoints in ISR will disrupt control loop; use non-intrusive methods

- **CAN Sniffing**: Monitor telemetry frames to observe state in real-time
  ```bash
  candump can0
  ```

## Comparison to Moteus (Parent Project)

SiPDrive is inspired by mjbots moteus but optimized for compactness:

| Feature | moteus (mjbots) | SiPDrive (yours) |
|---------|-----------------|---------------------|
| **MCU** | STM32G474 (512KB Flash, 128KB RAM) | STM32G431 (128KB Flash, 32KB RAM) |
| **Gate Driver** | DRV8323 (external chip) | STSPIN32G4 (integrated SiP) |
| **Current Sensing** | 3-phase (all ADCs) | 2-phase (Ia, Ib only) |
| **Build System** | Bazel + mbed-os | CMake + bare-metal |
| **Control Rate** | 30kHz (configurable) | 40kHz (configurable) |
| **Communication** | CAN-FD + RS485 | CAN-FD only |
| **Encoder** | AS5047P (SPI, 14-bit) | MT6701 (ABZ + PWM capture, 14-bit) |
| **Form Factor** | Standalone PCB | Compact (motor-mounted) |
| **Code Maturity** | Production (tested extensively) | ⚠️ Alpha (untested) |
| **Binary Size** | ~100KB+ | 11KB (8.6% of 128KB) |
| **RAM Usage** | ~20-30KB | 13.4KB main RAM + 8KB CCM RAM |

**Key Takeaway**: SiPDrive is more integrated (SiP) with tighter safety/feature budget than production moteus, but current RAM headroom is now workable for PoC.

## To Add Feature List (PoC → Functional Firmware)

### P0: Fundamental bring-up + controllability
1. **CAN-FD robustness**
   - Guarantee FD payload sizing (`RXESC/TXESC`) for >8-byte frames.
   - Add startup self-check that command and telemetry IDs are sane.
2. **Host control tooling**
   - Minimal Python `python-can` script for:
     - current-mode commands
     - position-mode commands
     - calibration trigger
     - telemetry decode
3. **Safety minimums for bench use**
   - Bus undervoltage/overvoltage fault thresholds using existing VBAT ADC.
   - Watchdog (IWDG) with clear ownership in ISR/main.
   - Fault latching policy documented per fault type.
4. **Encoder mode closure**
   - Decide and lock MT6701 MODE hardware strap.
   - Validate ABZ scaling and PWM absolute decode against ground truth.
5. **Commissioning flow**
   - Repeatable script/checklist: flash → verify telemetry → low-current spin → calibration.

### P1: Better control quality
6. **Motor characterization routines**
   - Phase resistance (R), phase inductance (L), and electrical direction check.
   - Optional Kv/Kt estimation workflow and storage.
7. **Loop-rate separation**
   - Keep current loop at 40kHz.
   - Run velocity/position loops at lower fixed divisors.
8. **Telemetry expansion**
   - Add electrical angle, duty, loop errors, and fault cause code.

### P2: Toward production-ready
9. **Control deadline monitoring**
   - Detect ISR overruns and force safe state.
10. **DMA for regular ADC paths**
    - Reduce jitter and CPU load in main loop.
11. **Advanced control features**
    - Field weakening, MTPA, and optional sensorless estimator.

## Future Enhancements

### High Priority

1. **Stack profiling**
   - Profile actual stack usage during runtime.
   - Reduce `_Min_Stack_Size` if current 4KB allocation is excessive.

2. **CAN-FD data-length hardening**
   - Explicitly set FD payload sizes in message RAM element configuration.
   - Verify 16-byte and 24-byte command/telemetry paths on real hardware.

3. **Safety baseline**
   - Add IWDG watchdog.
   - Add VBUS undervoltage/overvoltage faults.
   - Add control-loop deadline monitoring and latched fault behavior.

### Medium Priority

4. **DMA for ADC**: Use DMA to transfer ADC results, reduce ISR overhead
5. **Velocity Estimation**: Implement better velocity filtering (e.g., Kalman filter)
6. **Field Weakening**: Extend speed range with negative Id injection
7. **Math Function Accuracy**: Replace Taylor series with CORDIC or look-up tables
8. **Motor Characterization**: Add R/L/Kv/Kt commissioning routines

### Low Priority

9. **Sensorless Mode**: Back-EMF observer for encoder-free operation (requires significant flash)
10. **CAN Bootloader**: Allow firmware updates via CAN without debug probe
11. **Advanced Calibration**: Multi-turn absolute encoder support, temperature compensation
12. **Unit Tests**: Host-side tests for FOC math, configuration parsing

### Known Limitations

- **2-phase current sensing**: Cannot detect all fault modes (3-phase sensing would require more ADCs)
- **No hardware overcurrent protection**: Relies on software current limiting only
- **Fixed PWM frequency**: Not runtime configurable (requires recompilation)
- **No FOC decoupling terms**: Does not account for back-EMF or cross-coupling (Vd/Vq feedforward)
- **Feature gaps for production**: watchdog, voltage faults, deadline monitoring, robust host tooling
- **Math approximations**: Taylor series trig functions may be insufficiently accurate for high-performance FOC

## References

- **STM32G4 Reference Manual**: RM0440 (`hw/SiPDrive/datasheets/`)
- **STSPIN32G4 Datasheet**: (`hw/SiPDrive/datasheets/stspin32g4.pdf`)
- **MT6701 Encoder Datasheet**: (`hw/SiPDrive/datasheets/MT6701.pdf`)
- **FOC Theory**: "Vector Control of AC Machines" (Peter Vas)
- **SVPWM**: Application Note AN4013 (ST Microelectronics)

## Contact and Support

For questions about this codebase, refer to:
- Hardware design: `hw/SiPDrive/` KiCad files + datasheets
- Firmware architecture: This document + source code comments
- Build issues: `CMakeLists.txt` + linker script

---

## Document Status

**Last Updated**: 2026-02-14

**Build Status**: ✅ Compiles successfully
- Binary: 11.06KB Flash, 13.34KB RAM (+8KB CCMRAM)
- Toolchain: ARM GCC 15.2.0 (macOS Homebrew)
- Dependencies: CMSIS 5.9.0 + STM32G4 Device
- Build artifacts: `SiPDrive.elf`, `SiPDrive.bin`, `SiPDrive.hex`, `SiPDrive.map`

**Firmware Version**: Development (pre-v1.0)

**Code Quality**: ⚠️ **UNVERIFIED**
- AI-generated code
- Never executed on hardware
- No unit tests or simulation
- Math functions use approximations (Taylor series)
- Some peripheral/feature paths still pending on-hardware validation (notably CAN-FD >8-byte payload behavior)

**Hardware/Firmware Alignment**: ✅ **VALIDATED**
- Pin assignments verified against KiCad netlist (2026-02-09), renamed-file consistency checked (2026-02-14)
- Component datasheets reviewed
- Current sense parameters confirmed (5mΩ shunt, 16x OPAMP gain)
- Encoder wiring validated for ABZ + PWM; MT6701 MODE pin currently unconnected in netlist

**Safety Status**: 🔴 **NOT READY FOR PRODUCTION**
- No hardware testing performed
- Current sensing polarity unverified
- Electrical angle offset uncalibrated
- Math function accuracy not validated
- Watchdog/voltage/deadline fault coverage incomplete

**Recommended Next Steps**:
1. Run PoC bring-up workflow: flash, CAN-FD link setup (1M/5M), telemetry verify, low-current command
2. Add/verify CAN-FD FD payload sizing (`RXESC/TXESC`) for robust >8-byte frames
3. Bench validate current polarity and encoder direction before closing higher-level loops
4. Calibrate encoder electrical offset + compensation table and persist to flash
5. Add safety baseline: watchdog, UV/OV faults, and control-loop deadline monitoring
