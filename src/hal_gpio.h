#pragma once

#include <stdint.h>

#include "stm32g431xx.h"

namespace sipdrive::hal::gpio {

enum class Pull : uint8_t {
  kNone = 0,
  kUp = 1,
  kDown = 2,
};

void EnablePort(GPIO_TypeDef* port);
void ConfigureInput(GPIO_TypeDef* port, uint8_t pin, Pull pull);
void ConfigureOutput(GPIO_TypeDef* port, uint8_t pin, bool push_pull);
void ConfigureAlternate(GPIO_TypeDef* port, uint8_t pin, uint8_t af, Pull pull);
void ConfigureAnalog(GPIO_TypeDef* port, uint8_t pin);
void Write(GPIO_TypeDef* port, uint8_t pin, bool high);
bool Read(GPIO_TypeDef* port, uint8_t pin);

}  // namespace sipdrive::hal::gpio
