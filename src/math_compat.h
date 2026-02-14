#pragma once

// Math compatibility header for bare-metal ARM
// Provides basic math functions when standard library is not available

#ifndef NAN
#define NAN __builtin_nanf("")
#endif

#ifndef INFINITY
#define INFINITY __builtin_inff()
#endif

// Declare external math functions provided by libgcc
extern "C" {
  float fabsf(float x);
  float sqrtf(float x);
  float sinf(float x);
  float cosf(float x);
  float atan2f(float y, float x);
  float fmodf(float x, float y);
}

// Use compiler built-ins for these
inline bool isnan(float x) { return __builtin_isnan(x); }
inline bool isinf(float x) { return __builtin_isinf(x); }
