set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)
set(CMAKE_AR arm-none-eabi-ar)
set(CMAKE_OBJCOPY arm-none-eabi-objcopy)
set(CMAKE_SIZE arm-none-eabi-size)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(SIPDRIVE_CPU_FLAGS "-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard")

set(CMAKE_C_FLAGS_INIT "${SIPDRIVE_CPU_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT "${SIPDRIVE_CPU_FLAGS}")
set(CMAKE_ASM_FLAGS_INIT "${SIPDRIVE_CPU_FLAGS} -x assembler-with-cpp")
set(CMAKE_EXE_LINKER_FLAGS_INIT "${SIPDRIVE_CPU_FLAGS} -nostdlib -lgcc")

# Ensure ASM files use the ARM assembler, not the macOS native one
set(CMAKE_ASM_COMPILER ${CMAKE_C_COMPILER})
