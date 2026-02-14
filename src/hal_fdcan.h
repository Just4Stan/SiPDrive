#pragma once

#include <stdint.h>

namespace sipdrive::hal::fdcan {

struct CanFrame {
  uint32_t id;
  uint8_t len;
  uint8_t data[64];
  bool fd;
  bool brs;
};

void Init();
bool Receive(CanFrame* frame);
bool Send(const CanFrame& frame);
void Service();
void IrqHandler();
bool FaultActive();

}  // namespace sipdrive::hal::fdcan
