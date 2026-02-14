cmake_minimum_required(VERSION 3.20)
project(SiPDrive LANGUAGES C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(SIPDRIVE_USE_PLL170 "Use PLL170 as system clock source (default ON)" ON)
option(SIPDRIVE_USE_BKIN "Enable TIM1 BKIN hardware break input on PE15" OFF)
option(SIPDRIVE_DEBUG_TIMING "Enable GPIO timing markers for ISR/ADC debug" OFF)

if(SIPDRIVE_USE_PLL170)
  set(SIPDRIVE_SYS_CLOCK_HZ_DEFAULT 170000000)
else()
  set(SIPDRIVE_SYS_CLOCK_HZ_DEFAULT 16000000)
endif()

set(SIPDRIVE_SYS_CLOCK_HZ "${SIPDRIVE_SYS_CLOCK_HZ_DEFAULT}" CACHE STRING "System clock in Hz")
set(SIPDRIVE_APB2_PRESCALER "1" CACHE STRING "APB2 prescaler value")
set(SIPDRIVE_APB1_PRESCALER "1" CACHE STRING "APB1 prescaler value")
set(SIPDRIVE_CONTROL_LOOP_HZ "40000" CACHE STRING "Control loop frequency in Hz")
set(SIPDRIVE_PWM_FREQUENCY_HZ "40000" CACHE STRING "TIM1 PWM frequency in Hz")
set(SIPDRIVE_TIM1_DEADTIME_NS "250" CACHE STRING "TIM1 deadtime target in ns (encoded at runtime)")
set(SIPDRIVE_ADC_SAMPLE_OFFSET_TICKS "0" CACHE STRING "TIM1 CCR4 offset from ARR/2 for ADC trigger point")
set(SIPDRIVE_REGULAR_ADC_RATE_HZ "1000" CACHE STRING "Regular ADC service rate in Hz (<=1000 recommended)")
set(SIPDRIVE_ADC_IA_INJECTED_CHANNEL "13" CACHE STRING "ADC injected channel for Ia")
set(SIPDRIVE_ADC_IB_INJECTED_CHANNEL "16" CACHE STRING "ADC injected channel for Ib")
set(SIPDRIVE_ENCODER_CPR "16384" CACHE STRING "Encoder counts per rev placeholder (MT6701 ABI TBD)")
set(SIPDRIVE_MT6701_MODE_HIGH "1" CACHE STRING "MT6701 MODE pin level: 1=SSI/I2C, 0=ABZ")

set(SIPDRIVE_CMSIS_CORE_DIR "" CACHE PATH "Path to CMSIS Core include directory (contains core_cm4.h)")
set(SIPDRIVE_STM32G4_DEVICE_DIR "" CACHE PATH "Path to STM32G4 CMSIS device directory (contains Include/stm32g431xx.h)")

if(NOT SIPDRIVE_CMSIS_CORE_DIR)
  set(SIPDRIVE_CMSIS_CORE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/CMSIS/Core/Include")
endif()

if(NOT SIPDRIVE_STM32G4_DEVICE_DIR)
  set(SIPDRIVE_STM32G4_DEVICE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/cmsis_device_g4")
endif()

if(NOT EXISTS "${SIPDRIVE_CMSIS_CORE_DIR}/core_cm4.h")
  message(FATAL_ERROR "CMSIS core headers not found. Set SIPDRIVE_CMSIS_CORE_DIR to a directory containing core_cm4.h")
endif()

if(NOT EXISTS "${SIPDRIVE_STM32G4_DEVICE_DIR}/Include/stm32g431xx.h")
  message(FATAL_ERROR "STM32G4 device headers not found. Set SIPDRIVE_STM32G4_DEVICE_DIR to a cmsis-device-g4 root directory")
endif()

add_executable(SiPDrive.elf
  src/startup_stm32g431xx.s
  src/system_stm32g4xx.c
  src/main.cc
  src/hal_gpio.cc
  src/hal_tim1_pwm.cc
  src/hal_adc_opamp.cc
  src/hal_fdcan.cc
  src/hal_qei.cc
  src/hal_i2c3_stspin.cc
  src/hal_flash.cc
  src/foc_core.cc
  src/thermal.cc
  src/persistent_config.cc
  src/calibration.cc
  src/position_control.cc
  src/runtime_support.cc
)

if(SIPDRIVE_USE_PLL170)
  set(SIPDRIVE_USE_PLL170_VALUE 1)
else()
  set(SIPDRIVE_USE_PLL170_VALUE 0)
endif()

if(SIPDRIVE_USE_BKIN)
  set(SIPDRIVE_USE_BKIN_VALUE 1)
else()
  set(SIPDRIVE_USE_BKIN_VALUE 0)
endif()

if(SIPDRIVE_DEBUG_TIMING)
  set(SIPDRIVE_DEBUG_TIMING_VALUE 1)
else()
  set(SIPDRIVE_DEBUG_TIMING_VALUE 0)
endif()

set_target_properties(SiPDrive.elf PROPERTIES
  OUTPUT_NAME SiPDrive
  SUFFIX ".elf"
)

target_compile_definitions(SiPDrive.elf PRIVATE
  STM32G431xx
  SIPDRIVE_USE_PLL170=${SIPDRIVE_USE_PLL170_VALUE}
  SIPDRIVE_USE_BKIN=${SIPDRIVE_USE_BKIN_VALUE}
  SIPDRIVE_SYS_CLOCK_HZ=${SIPDRIVE_SYS_CLOCK_HZ}
  SIPDRIVE_APB1_PRESCALER=${SIPDRIVE_APB1_PRESCALER}
  SIPDRIVE_APB2_PRESCALER=${SIPDRIVE_APB2_PRESCALER}
  SIPDRIVE_CONTROL_LOOP_HZ=${SIPDRIVE_CONTROL_LOOP_HZ}
  SIPDRIVE_PWM_FREQUENCY_HZ=${SIPDRIVE_PWM_FREQUENCY_HZ}
  SIPDRIVE_TIM1_DEADTIME_NS=${SIPDRIVE_TIM1_DEADTIME_NS}
  SIPDRIVE_ADC_SAMPLE_OFFSET_TICKS=${SIPDRIVE_ADC_SAMPLE_OFFSET_TICKS}
  SIPDRIVE_REGULAR_ADC_RATE_HZ=${SIPDRIVE_REGULAR_ADC_RATE_HZ}
  SIPDRIVE_ADC_IA_INJECTED_CHANNEL=${SIPDRIVE_ADC_IA_INJECTED_CHANNEL}
  SIPDRIVE_ADC_IB_INJECTED_CHANNEL=${SIPDRIVE_ADC_IB_INJECTED_CHANNEL}
  SIPDRIVE_ENCODER_CPR=${SIPDRIVE_ENCODER_CPR}
  SIPDRIVE_MT6701_MODE_HIGH=${SIPDRIVE_MT6701_MODE_HIGH}
  SIPDRIVE_DEBUG_TIMING=${SIPDRIVE_DEBUG_TIMING_VALUE}
)

target_include_directories(SiPDrive.elf PRIVATE
  src
  ${SIPDRIVE_CMSIS_CORE_DIR}
  ${SIPDRIVE_STM32G4_DEVICE_DIR}/Include
)

target_compile_options(SiPDrive.elf PRIVATE
  -Os
  -ffreestanding
  -ffunction-sections
  -fdata-sections
  $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
  $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
  $<$<COMPILE_LANGUAGE:CXX>:-fno-use-cxa-atexit>
  $<$<COMPILE_LANGUAGE:CXX>:-fno-threadsafe-statics>
  -Wall
  -Wextra
  -Werror
)

target_link_options(SiPDrive.elf PRIVATE
  -T${CMAKE_CURRENT_SOURCE_DIR}/link/stm32g431vb.ld
  -Wl,--gc-sections
  -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/SiPDrive.map
  -Wl,--print-memory-usage
)

target_link_libraries(SiPDrive.elf PRIVATE
  gcc
)

add_custom_command(TARGET SiPDrive.elf POST_BUILD
  COMMAND ${CMAKE_SIZE} $<TARGET_FILE:SiPDrive.elf>
  COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:SiPDrive.elf> ${CMAKE_CURRENT_BINARY_DIR}/SiPDrive.bin
  COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:SiPDrive.elf> ${CMAKE_CURRENT_BINARY_DIR}/SiPDrive.hex
)
