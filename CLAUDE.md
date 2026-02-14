# CLAUDE.md

All project documentation is consolidated in [README.md](README.md).

Key sections for development context:
- **Firmware Architecture**: source layout, startup sequence, control loop ISR, HAL abstraction
- **FOC Algorithm**: Clarke/Park transforms, PI controllers, SVPWM
- **CAN-FD Protocol**: command and telemetry frame formats
- **Configuration**: compile-time (`config.h`) and persistent (flash) parameters
- **Build System Details**: toolchain, dependencies, memory layout, known issues
- **Schematic Audit**: current hardware status and required fixes
- **Roadmap**: prioritized feature plan (P0-P2)
- **Code Style**: naming conventions, C++17, namespace structure

## Quick Reference

- Build: `cmake --build build`
- Flash: `st-flash write build/SiPDrive.bin 0x08000000`
- Source: `src/` (firmware), `hw/SiPDrive/` (KiCad)
- Config: `src/config.h` (compile-time), `src/persistent_config.h` (flash)
- Control loop: `main.cc` ControlLoopIsr (40 kHz), calls `foc_core.cc`
- CAN: command ID 0x101, telemetry ID 0x181
- C++17, PascalCase functions, snake_case variables, kPascalCase constants
- Namespaces: `sipdrive::foc`, `sipdrive::hal::*`
