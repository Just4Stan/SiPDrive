// Minimal runtime support for bare-metal ARM
// Provides math and string functions when standard library is not available

#include <stddef.h>

extern "C" {

// ========== String functions ==========

void* memset(void* dest, int ch, size_t count) {
  unsigned char* d = static_cast<unsigned char*>(dest);
  const unsigned char c = static_cast<unsigned char>(ch);
  for (size_t i = 0; i < count; ++i) {
    d[i] = c;
  }
  return dest;
}

void* memcpy(void* dest, const void* src, size_t count) {
  unsigned char* d = static_cast<unsigned char*>(dest);
  const unsigned char* s = static_cast<const unsigned char*>(src);
  for (size_t i = 0; i < count; ++i) {
    d[i] = s[i];
  }
  return dest;
}

void* memmove(void* dest, const void* src, size_t count) {
  unsigned char* d = static_cast<unsigned char*>(dest);
  const unsigned char* s = static_cast<const unsigned char*>(src);

  if (d < s) {
    for (size_t i = 0; i < count; ++i) {
      d[i] = s[i];
    }
  } else {
    for (size_t i = count; i > 0; --i) {
      d[i-1] = s[i-1];
    }
  }
  return dest;
}

int memcmp(const void* lhs, const void* rhs, size_t count) {
  const unsigned char* l = static_cast<const unsigned char*>(lhs);
  const unsigned char* r = static_cast<const unsigned char*>(rhs);
  for (size_t i = 0; i < count; ++i) {
    if (l[i] != r[i]) {
      return (l[i] < r[i]) ? -1 : 1;
    }
  }
  return 0;
}

// ========== Math functions ==========
// Minimal implementations using Taylor series / CORDIC approximations

float fabsf(float x) {
  return (x < 0.0f) ? -x : x;
}

float sqrtf(float x) {
  if (x <= 0.0f) return 0.0f;

  // Newton-Raphson method
  float guess = x;
  for (int i = 0; i < 10; ++i) {
    guess = 0.5f * (guess + x / guess);
  }
  return guess;
}

// Simple polynomial approximation for sin (good for -pi to pi)
float sinf(float x) {
  const float pi = 3.14159265359f;

  // Normalize to [-pi, pi]
  while (x > pi) x -= 2.0f * pi;
  while (x < -pi) x += 2.0f * pi;

  // Taylor series: sin(x) ≈ x - x³/6 + x⁵/120 - x⁷/5040
  const float x2 = x * x;
  const float x3 = x2 * x;
  const float x5 = x3 * x2;
  const float x7 = x5 * x2;

  return x - x3/6.0f + x5/120.0f - x7/5040.0f;
}

float cosf(float x) {
  const float pi = 3.14159265359f;
  return sinf(x + pi/2.0f);
}

float atan2f(float y, float x) {
  const float pi = 3.14159265359f;

  if (x == 0.0f) {
    if (y > 0.0f) return pi / 2.0f;
    if (y < 0.0f) return -pi / 2.0f;
    return 0.0f;
  }

  float atan = y / x;

  // Simple approximation
  if (x > 0.0f) {
    return atan;
  } else if (y >= 0.0f) {
    return atan + pi;
  } else {
    return atan - pi;
  }
}

float fmodf(float x, float y) {
  if (y == 0.0f) return 0.0f;

  const float quot = static_cast<int>(x / y);
  return x - quot * y;
}

}  // extern "C"
