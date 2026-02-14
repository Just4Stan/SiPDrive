#include "hal_gpio.h"

namespace sipdrive::hal::gpio {
namespace {

void SetPull(GPIO_TypeDef* port, uint8_t pin, Pull pull) {
  const uint32_t shift = static_cast<uint32_t>(pin) * 2U;
  port->PUPDR = (port->PUPDR & ~(0x3UL << shift)) |
                (static_cast<uint32_t>(pull) << shift);
}

}  // namespace

void EnablePort(GPIO_TypeDef* port) {
  if (port == GPIOA) {
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
  } else if (port == GPIOB) {
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOBEN;
  } else if (port == GPIOC) {
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOCEN;
  } else if (port == GPIOD) {
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIODEN;
  } else if (port == GPIOE) {
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOEEN;
  } else if (port == GPIOF) {
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOFEN;
  }
  (void)RCC->AHB2ENR;
}

void ConfigureInput(GPIO_TypeDef* port, uint8_t pin, Pull pull) {
  const uint32_t shift = static_cast<uint32_t>(pin) * 2U;
  port->MODER &= ~(0x3UL << shift);
  SetPull(port, pin, pull);
}

void ConfigureOutput(GPIO_TypeDef* port, uint8_t pin, bool push_pull) {
  const uint32_t shift = static_cast<uint32_t>(pin) * 2U;
  const uint32_t pin_mask = 1UL << pin;

  port->MODER = (port->MODER & ~(0x3UL << shift)) | (0x1UL << shift);
  if (push_pull) {
    port->OTYPER &= ~pin_mask;
  } else {
    port->OTYPER |= pin_mask;
  }
  port->OSPEEDR = (port->OSPEEDR & ~(0x3UL << shift)) | (0x2UL << shift);
  SetPull(port, pin, Pull::kNone);
}

void ConfigureAlternate(GPIO_TypeDef* port, uint8_t pin, uint8_t af, Pull pull) {
  const uint32_t shift = static_cast<uint32_t>(pin) * 2U;
  const uint32_t afr_idx = pin / 8U;
  const uint32_t afr_shift = (pin % 8U) * 4U;

  port->MODER = (port->MODER & ~(0x3UL << shift)) | (0x2UL << shift);
  port->AFR[afr_idx] = (port->AFR[afr_idx] & ~(0xFUL << afr_shift)) |
                       ((static_cast<uint32_t>(af) & 0xFUL) << afr_shift);
  port->OSPEEDR = (port->OSPEEDR & ~(0x3UL << shift)) | (0x2UL << shift);
  port->OTYPER &= ~(1UL << pin);
  SetPull(port, pin, pull);
}

void ConfigureAnalog(GPIO_TypeDef* port, uint8_t pin) {
  const uint32_t shift = static_cast<uint32_t>(pin) * 2U;
  port->MODER = (port->MODER & ~(0x3UL << shift)) | (0x3UL << shift);
  port->PUPDR &= ~(0x3UL << shift);
}

void Write(GPIO_TypeDef* port, uint8_t pin, bool high) {
  const uint32_t mask = 1UL << pin;
  port->BSRR = high ? mask : (mask << 16U);
}

bool Read(GPIO_TypeDef* port, uint8_t pin) {
  return (port->IDR & (1UL << pin)) != 0U;
}

}  // namespace sipdrive::hal::gpio
