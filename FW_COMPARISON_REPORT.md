# SiPDrive vs moteus `fw` Gap Report

Date: 2026-02-14
Repository: `SiPDrive`
Compared paths:
- `src`
- `fw`

Rebrand note:
- This report content is still technically valid after the project rename to `SiPDrive`.
- Protocol, control-loop, safety, and test-scope gaps versus `moteus/fw` are unchanged by naming.

## Executive Summary

`SiPDrive` is currently a compact, board-specific firmware focused on:
- 40kHz FOC current loop
- Basic position loop
- Fixed CAN command/telemetry frames
- Single encoder path (QEI + index, optional MT6701 PWM capture)
- Basic thermal and fault gating

The full `fw` is a significantly broader platform with:
- Rich control modes and trajectories
- Register-based protocol over multiplex
- Multi-sensor/multi-port encoder and aux IO stack
- Extensive persistent config surface
- Bootloader and large test/simulation coverage

`SiPDrive` should not copy all of `fw` at once. The most effective path is to add protocol compatibility, stronger safety/limits, better config surface, and test coverage first.

## Current Size and Complexity Delta

- `src`: ~3.7k LOC, 31 files
- `fw` (excluding `fw/test`): ~21.7k LOC, 100+ files
- `fw/test`: ~8.7k LOC

Interpretation:
- `SiPDrive` is intentionally minimal.
- Feature parity with `fw` requires staged scope control to avoid instability and RAM overrun.

## Functional Differences

## 1) Control and Motion

Present in `SiPDrive`:
- Mode set: current + position only
- Position loop: simple position-to-velocity + velocity PI to `iq`
- Basic FOC current PI and SVPWM-like duty generation

Present in `fw`, missing in `SiPDrive`:
- Additional modes: PWM, phase voltage, voltage FOC, voltage DQ, position timeout, zero-velocity, stay-within-bounds, brake, inductance measurement
- Advanced trajectory handling with velocity/acceleration constraints and completion logic
- Torque model (nonlinear high-current behavior) and richer feedforward options
- More complete limit/fallback behavior across modes

Why it matters:
- Limits dynamic behavior, fallback strategies, and tuning flexibility.
- Makes behavior less consistent with ecosystem tools expecting moteus-like semantics.

## 2) Protocol and Command Surface

Present in `SiPDrive`:
- Custom fixed CAN-FD command frame (16-byte) + legacy 4-byte current frame
- Fixed telemetry frame (24-byte)

Present in `fw`, missing in `SiPDrive`:
- Register map protocol (versioned) with read/write typed scaling
- Multiplex server integration and tunnel streams
- Broad command/telemetry namespace (servo, motor, aux, firmware metadata, etc.)
- Dynamic CAN prefix/filter behavior via persistent config

Why it matters:
- Limits host tooling compatibility.
- Harder to extend without protocol breaks.

## 3) Configuration and Persistence

Present in `SiPDrive`:
- Single persistent struct with CRC and defaults
- Core fields: motor/limits, PID subset, thermal limits, CAN IDs, calibration table

Present in `fw`, missing in `SiPDrive`:
- Modular persistent configuration domains (`servo`, `servopos`, `motor_position`, `aux`, `id`, `can`, etc.)
- Much larger tunable surface for control/limits/fault behaviors
- Protocol-driven runtime introspection and mutation of config

Why it matters:
- Current config is practical for bring-up but not for broad deployment/tuning workflows.

## 4) Sensing, Encoders, and Aux IO

Present in `SiPDrive`:
- QEI incremental encoder + index reset
- Optional MT6701 PWM absolute decode
- STSPIN init over I2C
- Basic ADC path for currents + VBAT/NTCs

Present in `fw`, missing in `SiPDrive`:
- Multi-source position pipeline (SPI/UART/quadrature/hall/index/sine-cosine/I2C/BiSS-C/sensorless)
- Dual AUX port infrastructure with configurable pin modes and device protocols
- Rich source validation/filtering/failover in motor position stack

Why it matters:
- Limits hardware compatibility and robustness in edge cases.

## 5) Safety, Faults, and Diagnostics

Present in `SiPDrive`:
- Hardware fault gate (`nFAULT`) + thermal fault + CAN health bit
- Thermal derating and cutoff
- Basic fault bits in telemetry

Present in `fw`, missing in `SiPDrive`:
- Rich error taxonomy and limit causes
- Timing violation detection/faulting logic
- More detailed status exposure and diagnostics tools

Why it matters:
- Reduced observability and fault attribution.
- Harder to diagnose field issues quickly.

## 6) Build, Deployment, and Testing

Present in `SiPDrive`:
- CMake bare-metal build, small binary, simple flow
- No dedicated unit/simulation test suite currently in-tree

Present in `fw`, missing in `SiPDrive`:
- Bootloader target and full flash packaging flow
- Broad C++ and Python test suite, including simulation/regression tests
- Mature host-tool integration points

Why it matters:
- Higher regression risk as features are added.
- Update/deployment path less mature.

## What Should Be Added to SiPDrive (Prioritized)

## Priority 0: Protect headroom before feature growth

1. Add RAM/flash budget checks in CI-style scripts.
2. Add minimal hardware-in-loop smoke tests for:
   - control ISR alive
   - CAN Rx/Tx
   - encoder read sanity
   - overtemp shutdown path
3. Track control loop timing budget and ISR overruns.

Reason:
- Current documented RAM usage is already high; feature additions can break stability silently.

## Priority 1: Protocol compatibility layer (highest value)

1. Implement a compact register-based command/telemetry layer compatible with key moteus registers.
2. Keep existing fixed frames as legacy mode during transition.
3. Add register map version field and scaling rules.

Minimum useful register subset:
- mode
- position/velocity/torque/current command fields
- fault, temperature, voltage, position, velocity feedback
- firmware/register map version

Reason:
- Gives immediate host tooling interoperability without full architecture rewrite.

## Priority 2: Safety and limit behavior parity

1. Add structured fault codes (not just bit flags).
2. Add timing violation detection.
3. Add explicit command timeout behavior (configurable fallback mode).
4. Add stronger position/velocity/torque bounding semantics.

Reason:
- Direct reliability gains with modest code size increase.

## Priority 3: Control loop enhancements

1. Add velocity/acceleration trajectory shaping from position commands.
2. Add configurable feedforward terms (starting with velocity feedforward).
3. Add optional torque model conversion path.

Reason:
- Improves real servo quality and closes much of the user-visible behavior gap.

## Priority 4: Config and calibration expansion

1. Split persistent config into domains (servo, position, motor, can, id).
2. Add schema version migration helpers.
3. Expand calibration metadata and validation flags.

Reason:
- Required for maintainable long-term growth and remote configuration tooling.

## Priority 5: Sensor and IO breadth

1. Add one additional encoder path first (SPI encoder) with clear arbitration against QEI.
2. Add minimal AUX pin mode support (digital in/out, analog in).
3. Extend to more sensor types only after protocol + safety layers are stable.

Reason:
- High implementation cost; defer until core platform behavior is stable.

## Priority 6: Bootloader/update path and broader test coverage

1. Add a minimal CAN bootloader/update flow.
2. Add host tests for protocol encode/decode and limits behavior.
3. Add regression playback tests for control behavior where possible.

Reason:
- Necessary for production deployment and safe iteration.

## Recommended Implementation Order

1. Runtime budget checks + smoke tests
2. Protocol compatibility layer (subset register map)
3. Fault/timeout/limit behavior
4. Trajectory + feedforward improvements
5. Config schema expansion
6. Additional sensors/AUX
7. Bootloader + full regression suite

## Scope Guidance

- Do not attempt full `fw` parity in one branch.
- Keep `SiPDrive` architecture lean; import concepts, not full subsystem complexity.
- Gate each phase with measurable criteria:
  - loop timing unchanged within target margin
  - no RAM regression beyond budget
  - protocol tests passing
  - no fault-handling regressions in smoke tests

## Suggested Immediate Next Milestone (2-3 weeks)

Deliverable:
- Register-based protocol subset
- Structured fault codes + timeout handling
- Basic protocol/unit tests

Success criteria:
- Existing fixed-frame clients still work
- At least one moteus-style client path can command/read key registers
- No control-rate instability introduced
