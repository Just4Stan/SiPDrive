# Build Notes

**Project**: SiPDrive
**Date**: 2026-02-14
**Platform**: macOS (Darwin 25.2.0)
**Status**: ✅ Builds successfully

## Quick Build

```bash
# From repository root
cmake -B build \
  -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi-gcc.cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DSIPDRIVE_CMSIS_CORE_DIR=external/CMSIS/CMSIS/Core/Include \
  -DSIPDRIVE_STM32G4_DEVICE_DIR=external/cmsis_device_g4

cmake --build build --target SiPDrive.elf
```

## Build Outputs

Canonical outputs:
- `build/SiPDrive.elf`
- `build/SiPDrive.bin`
- `build/SiPDrive.hex`
- `build/SiPDrive.map`

## Current Build Statistics

From build on 2026-02-14:

```
Flash:    11,056 bytes / 128 KB (8.44%)
RAM:      13,344 bytes /  22 KB (59.23%)
CCM RAM:   8,192 bytes /  10 KB (80.00%)

Binary size: 11 KB
ELF size:    42 KB (with debug symbols)
```

## Toolchain

- ARM GCC 15.2.0 (`/opt/homebrew/bin/arm-none-eabi-gcc`)
- CMake 4.2.3 (`/opt/homebrew/bin/cmake`)
- Binutils (`/opt/homebrew/opt/arm-none-eabi-binutils/bin/`)

Install:

```bash
brew install arm-none-eabi-gcc cmake
```

## Dependencies

Expected in `external/`:
- `external/CMSIS` (CMSIS Core headers)
- `external/cmsis_device_g4` (STM32G4 device headers)

Fetch:

```bash
cd external
git clone --depth 1 --branch 5.9.0 https://github.com/ARM-software/CMSIS_5.git CMSIS
git clone --depth 1 https://github.com/STMicroelectronics/cmsis_device_g4.git
```

## Known Build Caveats

1. FDCAN `RXESC/TXESC` register definitions are absent in current STM32G431 CMSIS headers used by this project.
2. `runtime_support.cc` provides math/string fallbacks due toolchain environment constraints.
3. Build is validated, but hardware execution paths are still pending full bench verification.

## Clean Rebuild

```bash
rm -rf build
cmake -B build \
  -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi-gcc.cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DSIPDRIVE_CMSIS_CORE_DIR=external/CMSIS/CMSIS/Core/Include \
  -DSIPDRIVE_STM32G4_DEVICE_DIR=external/cmsis_device_g4
cmake --build build --target SiPDrive.elf
```

## Verification Commands

```bash
arm-none-eabi-size -A build/SiPDrive.elf
arm-none-eabi-nm -C -S --size-sort build/SiPDrive.elf > symbols.txt
```
