#pragma once

#include <stddef.h>

// Minimal string.h compatibility for bare-metal ARM

extern "C" {
  void* memset(void* dest, int ch, size_t count);
  void* memcpy(void* dest, const void* src, size_t count);
  void* memmove(void* dest, const void* src, size_t count);
  int memcmp(const void* lhs, const void* rhs, size_t count);
}  // extern "C"
