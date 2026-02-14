# SiPDrive

SiPDrive is a compact open-source FOC servo controller firmware and hardware project for QDD-style actuators.

It targets an STM32G431 + STSPIN32G4-based controller with:
- MT6701 encoder support (ABZ/SSI path in firmware)
- CAN-FD command/telemetry
- 40 kHz current loop (FOC)
- Basic position + current control modes
- On-device encoder calibration + persistent config
- Thermal derating/fault handling

## Current Progress (2026-02-14)

Status:
- Firmware builds cleanly on macOS ARM GCC toolchain
- Rebrand from legacy naming completed across firmware, docs, and KiCad project files

Latest build footprint:
- Flash: `11,056 B / 128 KB` (8.44%)
- RAM: `13,344 B / 22 KB` (59.23%)
- CCM RAM: `8,192 B / 10 KB` (80.00%)

Validation state:
- Compile-tested
- Not yet fully validated on hardware bring-up bench

## Repository Layout

- `src/`: firmware source
- `hw/SiPDrive/`: KiCad project, production outputs, datasheets
- `cmake/`: toolchain configuration
- `link/`: linker script
- `external/`: CMSIS dependencies (not vendored by default)

## Build Quick Start

Prereqs:
- `arm-none-eabi-gcc`
- `cmake`

Build:

```bash
cmake -B build \
  -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi-gcc.cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DSIPDRIVE_CMSIS_CORE_DIR=external/CMSIS/CMSIS/Core/Include \
  -DSIPDRIVE_STM32G4_DEVICE_DIR=external/cmsis_device_g4

cmake --build build --target SiPDrive.elf
```

Outputs:
- Canonical: `build/SiPDrive.elf`, `.bin`, `.hex`, `.map`

## Bring-Up Notes

Primary interfaces:
- CAN-FD nominal/data: 1 Mbps / 5 Mbps
- Command IDs default to `0x101`, telemetry `0x181`

Before powering a motor:
- verify current polarity
- verify encoder direction/offset
- start with low current limits
- confirm thermal/fault behavior

## Known Gaps

- No production-grade test suite yet (unit/simulation/HIL still pending)
- CAN-FD >8 byte payload path requires on-hardware confirmation with current CMSIS header constraints
- Safety coverage still incomplete for production deployment (watchdog/UV-OV/deadline faulting roadmap items)

## Key Docs

- `BUILD_NOTES.md`: toolchain and reproducible build details
- `FW_COMPARISON_REPORT.md`: gap analysis vs `moteus/fw`
- `CLAUDE.md`: deep architecture and implementation notes

## License

TBD (set before public release if not already defined in your intended upstream repo policy).
